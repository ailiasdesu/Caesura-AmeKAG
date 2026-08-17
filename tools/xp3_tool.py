#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
xp3_tool.py - KiriKiri (吉里吉里) .xp3 归档只读解析器（v1 最小原型）
=====================================================================

独立 CLI 工具，只读解析 .xp3 归档（KiriKiri / TVP XP3 虚拟文件系统格式），
不接入 Caesura 引擎。

v1 支持范围
-----------
* 未加密索引（raw index 与 zlib 压缩索引两种编码方法，见 index_flag bits 0-2）
* 单索引块（无 INDEX_CONTINUE 链式索引）
* 数据段两种编码：raw（不压缩）与 zlib（zlib 封装 deflate）
* 每个文件条目可含多个 Segment（首尾拼接还原）
* FLAGS bit31（protected）条目可列出；解压时给出警告但仍尝试
* 纯 .xp3 文件（文件头即 XP3 魔数）；不支持嵌入 .exe 的自解压变体

v1 明确不支持（会报清晰错误）
-----------------------------
* 加密索引（index_flag 编码方法不在 {0,1}，或 GARbro 风格 0x80 索引重定向标记）
* INDEX_CONTINUE 链式索引导航
* 段加密（segment flags bits0-2 非 raw/zlib）
* 非 BMP UTF-16 文件名字符（格式规范仅 BMP，忽略此限制）

用法
----
  python tools/xp3_tool.py list <archive.xp3> [--json]
  python tools/xp3_tool.py extract <archive.xp3> <hash|name> [--out DIR]
  python tools/xp3_tool.py extract-all <archive.xp3> [--out DIR] [--use-names]
  python tools/xp3_tool.py verify <archive.xp3>
"""

import argparse
import os
import re
import struct
import sys
import zlib

# ---------------------------------------------------------------------------
# 格式常量（与 krkrz base/XP3Archive.h 一致）
# ---------------------------------------------------------------------------

# 规范魔数（KiriKiri 权威）：'X','P','3',0x0d,0x0a,0x20,0x0a,0x1a,0x8b,0x67,0x01
# = "XP3\r\n \n\x1a\x8bg\x01" —— 注意 0x0a 后面是 0x20(空格) 而非 0x1a。
# 网络上常见引用的 "XP3\r\n\x1a\x8a\r\n\x00\x00" 是误传变体，
# 本工具对两种魔数都接受，但写入时使用规范魔数。
XP3_MAGIC_CANONICAL = b"XP3\r\n \n\x1a\x8bg\x01"
XP3_MAGIC_VARIANT   = b"XP3\r\n\x1a\x8a\r\n\x00\x00"
XP3_MAGICS = (XP3_MAGIC_CANONICAL, XP3_MAGIC_VARIANT)
XP3_MAGIC_LEN = 11

# index_flag（索引块起始 1 字节）
INDEX_ENCODE_METHOD_MASK = 0x07
INDEX_ENCODE_RAW         = 0x00
INDEX_ENCODE_ZLIB        = 0x01
INDEX_CONTINUE           = 0x80  # 链式索引 / GARbro 加密探测标记

# info 子块 flags（info 起始 4 字节）
FILE_PROTECTED = 1 << 31

# segm 段 flags（每段起始 4 字节）
SEGM_ENCODE_METHOD_MASK = 0x07
SEGM_ENCODE_RAW         = 0x00
SEGM_ENCODE_ZLIB        = 0x01

# 子块 4 字节标签
TAG_FILE = b"File"
TAG_INFO = b"info"
TAG_SEGM = b"segm"
TAG_ADLR = b"adlr"


class XP3Error(Exception):
    """带机器可读类别的解析错误。"""

    def __init__(self, kind, message):
        super().__init__(message)
        self.kind = kind
        self.message = message


def _u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def _u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def _u64(b, o):
    return struct.unpack_from("<Q", b, o)[0]


def _i16(b, o):
    return struct.unpack_from("<h", b, o)[0]


# ---------------------------------------------------------------------------
# 数据模型
# ---------------------------------------------------------------------------

class Segment:
    """数据段：定位 + 编码。"""

    __slots__ = ("start", "org_size", "arc_size", "compressed")

    def __init__(self, start, org_size, arc_size, compressed):
        self.start = start          # 段在归档文件内的绝对偏移
        self.org_size = org_size    # 解压后字节数
        self.arc_size = arc_size    # 归档中实际字节数
        self.compressed = compressed  # True=zlib, False=raw


class Entry:
    """一个归档文件条目。"""

    __slots__ = (
        "name", "path_hash", "info_flags", "org_size", "arc_size",
        "segments", "_decompressed_cache",
    )

    def __init__(self):
        self.name = None            # str 或 None（索引缺失名字时以哈希代称）
        self.path_hash = None       # adlr 子块 u32 路径哈希（引擎查找用，非数据校验和）
        self.info_flags = 0         # info 子块 flags
        self.org_size = 0           # info 原始大小（未压缩总长）
        self.arc_size = 0           # info 归档大小（压缩后总长）
        self.segments = []          # list[Segment]
        self._decompressed_cache = None

    @property
    def protected(self):
        return bool(self.info_flags & FILE_PROTECTED)

    @property
    def display_name(self):
        return self.name if self.name else "hash:0x%08x" % (self.path_hash or 0)

    def __repr__(self):
        return "<Entry %r org=%d arc=%d segs=%d>" % (
            self.display_name, self.org_size, self.arc_size, len(self.segments))


# ---------------------------------------------------------------------------
# 归档解析
# ---------------------------------------------------------------------------

class XP3Archive:
    """只读 .xp3 归档对象。"""

    def __init__(self, path, base_offset=0):
        self.path = path
        self.base_offset = base_offset      # 当前恒为 0（v1 不支持 exe 内嵌）
        self.magic_used = None              # 命中的魔数
        self.index_flag = None              # 索引块 flag 字节
        self.entries = []                   # list[Entry]
        self._data = None

        with open(path, "rb") as f:
            self._data = f.read()

    # -- 解析入口 -----------------------------------------------------------

    def parse(self):
        data = self._data

        if len(data) < XP3_MAGIC_LEN:
            raise XP3Error("bad-magic",
                           "文件过短（%d 字节），不是合法 XP3 归档" % len(data))
        head = data[:XP3_MAGIC_LEN]
        if head == XP3_MAGIC_CANONICAL:
            self.magic_used = "canonical"
        elif head == XP3_MAGIC_VARIANT:
            self.magic_used = "variant"
        else:
            raise XP3Error(
                "bad-magic",
                "魔数不匹配：期望 %r 或 %r，实际 %r"
                % (XP3_MAGIC_CANONICAL, XP3_MAGIC_VARIANT, head))

        if len(data) < XP3_MAGIC_LEN + 8:
            raise XP3Error("truncated-header",
                           "文件过短，缺少索引偏移字段")

        index_ofs = _u64(data, XP3_MAGIC_LEN)
        self._parse_index(index_ofs)
        return self

    # -- 索引块 -------------------------------------------------------------

    def _parse_index(self, index_ofs):
        data = self._data
        if index_ofs + 1 > len(data):
            raise XP3Error("bad-index-offset",
                           "索引偏移 0x%x 超出文件末尾" % index_ofs)

        flag = data[index_ofs]
        self.index_flag = flag

        # GARbro 风格加密/重定向索引标记：索引位置处 u32 == 0x80
        if flag == INDEX_CONTINUE and index_ofs + 4 <= len(data) \
                and _u32(data, index_ofs) == 0x80:
            raise XP3Error(
                "encrypted-index",
                "检测到加密/重定向索引标记（索引位置 0x%x 处 u32==0x80）；"
                "v1 只支持未加密索引" % index_ofs)

        method = flag & INDEX_ENCODE_METHOD_MASK
        if method not in (INDEX_ENCODE_RAW, INDEX_ENCODE_ZLIB):
            raise XP3Error(
                "encrypted-index",
                "索引编码方法 0x%x（flag=0x%02x）不受支持："
                "加密或未知编码索引；v1 只支持未加密索引（raw=0 / zlib=1）"
                % (method, flag))

        if flag & INDEX_CONTINUE:
            raise XP3Error(
                "index-continue-unsupported",
                "链式索引（INDEX_CONTINUE=0x%02x）在 v1 中不受支持" % flag)

        pos = index_ofs + 1
        if pos + 8 > len(data):
            raise XP3Error("truncated-index", "索引块大小字段截断")

        if method == INDEX_ENCODE_RAW:
            index_size = _u64(data, pos)
            pos += 8
            if pos + index_size > len(data):
                raise XP3Error("truncated-index",
                               "raw 索引数据越界（size=%d）" % index_size)
            indexdata = data[pos:pos + index_size]
        else:  # zlib
            compressed_size = _u64(data, pos)
            index_size = _u64(data, pos + 8)
            pos += 16
            if pos + compressed_size > len(data):
                raise XP3Error("truncated-index",
                               "zlib 索引压缩数据越界（size=%d）" % compressed_size)
            try:
                indexdata = zlib.decompress(data[pos:pos + compressed_size])
            except zlib.error as e:
                raise XP3Error("index-zlib-error",
                               "索引 zlib 解压失败：%s" % e)
            if len(indexdata) != index_size:
                raise XP3Error("index-size-mismatch",
                               "索引解压后大小 %d != 声明 %d"
                               % (len(indexdata), index_size))

        self._parse_index_payload(indexdata, index_ofs)

    def _parse_index_payload(self, indexdata, index_ofs):
        """在索引载荷中依次扫描顶层 'File' 块。"""
        size = len(indexdata)
        start = 0
        while start + 12 <= size:
            tag = indexdata[start:start + 4]
            chunk_size = _u64(indexdata, start + 4)
            if chunk_size > size - (start + 12):
                raise XP3Error("bad-index-chunk",
                               "顶层块 %r 大小 %d 越界" % (tag, chunk_size))
            if tag == TAG_FILE:
                if chunk_size < 12:
                    raise XP3Error("bad-index-chunk",
                                   "'File' 块大小 %d 过小" % chunk_size)
                entry = self._parse_file_chunk(indexdata, start + 12, chunk_size)
                if entry is not None:
                    self.entries.append(entry)
            # 其他顶层块（如 yuz:/sen:/dls: 名字映射等）v1 跳过
            start += 12 + chunk_size

        if not self.entries:
            raise XP3Error("empty-index",
                           "索引中未找到任何文件条目（归档可能损坏或为空）")

    def _parse_file_chunk(self, indexdata, base, size):
        """解析一个 'File' 块内的 info/segm/adlr 子块。"""
        entry = Entry()
        end = base + size
        pos = base
        have_info = False
        have_segm = False

        while pos + 12 <= end:
            tag = indexdata[pos:pos + 4]
            chunk_size = _u64(indexdata, pos + 4)
            payload = pos + 12
            if chunk_size > end - payload:
                raise XP3Error("bad-index-subchunk",
                               "子块 %r 大小 %d 越界" % (tag, chunk_size))

            if tag == TAG_INFO:
                if chunk_size < 22:
                    raise XP3Error("bad-info",
                                   "'info' 子块过小（%d < 22）" % chunk_size)
                entry.info_flags = _u32(indexdata, payload)
                entry.org_size = _u64(indexdata, payload + 4)
                entry.arc_size = _u64(indexdata, payload + 12)
                name_len = _i16(indexdata, payload + 20)
                if name_len < 0:
                    raise XP3Error("bad-info",
                                   "'info' 文件名长度字段为负（%d）" % name_len)
                if 22 + name_len * 2 > chunk_size:
                    raise XP3Error("bad-info",
                                   "'info' 文件名数据越界（len=%d）" % name_len)
                raw = indexdata[payload + 22:payload + 22 + name_len * 2]
                try:
                    entry.name = raw.decode("utf-16-le")
                except UnicodeDecodeError:
                    entry.name = None  # 无法解码则以哈希代称
                have_info = True

            elif tag == TAG_SEGM:
                if chunk_size % 28 != 0:
                    raise XP3Error("bad-segm",
                                   "'segm' 子块大小 %d 不是 28 的倍数" % chunk_size)
                n = chunk_size // 28
                for i in range(n):
                    p = payload + i * 28
                    seg_flags = _u32(indexdata, p)
                    method = seg_flags & SEGM_ENCODE_METHOD_MASK
                    if method == SEGM_ENCODE_RAW:
                        compressed = False
                    elif method == SEGM_ENCODE_ZLIB:
                        compressed = True
                    else:
                        raise XP3Error(
                            "encrypted-segment",
                            "段编码方法 0x%x 不受支持（加密或未知段编码）；"
                            "v1 只支持 raw=0 / zlib=1" % method)
                    start = _u64(indexdata, p + 4)
                    org_size = _u64(indexdata, p + 12)
                    arc_size = _u64(indexdata, p + 20)
                    entry.segments.append(
                        Segment(start, org_size, arc_size, compressed))
                have_segm = True

            elif tag == TAG_ADLR:
                if chunk_size >= 4:
                    entry.path_hash = _u32(indexdata, payload)

            pos = payload + chunk_size

        if not have_info or not have_segm:
            return None  # 不完整的 File 块，跳过
        return entry

    # -- 数据读取 -----------------------------------------------------------

    def _read_segment(self, seg):
        data = self._data
        start = self.base_offset + seg.start
        if start + seg.arc_size > len(data):
            raise XP3Error("segment-overflow",
                           "段偏移 0x%x + 大小 %d 超出文件末尾"
                           % (start, seg.arc_size))
        blob = data[start:start + seg.arc_size]
        if not seg.compressed:
            if seg.org_size != seg.arc_size:
                raise XP3Error("segment-size-mismatch",
                               "raw 段声明 org=%d 但 arc=%d" % (seg.org_size, seg.arc_size))
            return blob
        try:
            out = zlib.decompress(blob)
        except zlib.error:
            # 兼容个别工具写 raw-deflate（无 zlib 头）的情况
            try:
                out = zlib.decompress(blob, -15)
            except zlib.error as e:
                raise XP3Error("segment-zlib-error",
                               "段 zlib 解压失败：%s" % e)
        if len(out) != seg.org_size:
            raise XP3Error("segment-size-mismatch",
                           "段解压后 %d 字节 != 声明 org=%d"
                           % (len(out), seg.org_size))
        return out

    def read_entry(self, entry):
        """读出条目的完整还原内容（多段依次拼接）。"""
        if entry._decompressed_cache is None:
            parts = [self._read_segment(s) for s in entry.segments]
            entry._decompressed_cache = b"".join(parts)
        return entry._decompressed_cache

    def verify(self):
        """结构 + 数据完整性校验。返回统计 dict，失败抛 XP3Error。"""
        total_org = 0
        total_arc = 0
        n_compressed = 0
        for e in self.entries:
            self.read_entry(e)  # 触发解压与长度校验
            total_org += e.org_size
            total_arc += e.arc_size
            if any(s.compressed for s in e.segments):
                n_compressed += 1
        return {
            "files": len(self.entries),
            "total_original_bytes": total_org,
            "total_archived_bytes": total_arc,
            "compressed_files": n_compressed,
            "index_encode": "zlib" if (self.index_flag & INDEX_ENCODE_METHOD_MASK)
                            == INDEX_ENCODE_ZLIB else "raw",
        }


# ---------------------------------------------------------------------------
# 名字/哈希选择解析
# ---------------------------------------------------------------------------

_HASH_RE = re.compile(r"^(0x)?([0-9a-fA-F]{1,8})$")


def select_entry(archive, selector):
    """按 8 位十六进制哈希（可前缀）或文件名选择条目。"""
    m = _HASH_RE.match(selector.strip())
    if m:
        want = int(m.group(2), 16)
        matches = [e for e in archive.entries
                   if e.path_hash is not None and e.path_hash == want]
        if not matches:
            digits = m.group(2)
            if len(digits) >= 4:
                matches = [e for e in archive.entries
                           if e.path_hash is not None
                           and ("%08x" % e.path_hash).startswith(digits.lower())]
        if not matches:
            raise XP3Error("not-found", "未找到哈希 %s" % selector)
        if len(matches) > 1:
            raise XP3Error("ambiguous",
                           "哈希前缀 %s 匹配多个条目：%s"
                           % (selector,
                              ", ".join(e.display_name for e in matches)))
        return matches[0]

    # 按名字：先精确（规范化 /），再基线名
    norm = selector.replace("\\", "/").lstrip("/")
    for e in archive.entries:
        if e.name is not None and e.name.replace("\\", "/").lstrip("/") == norm:
            return e
    base = os.path.basename(norm)
    basename_matches = [e for e in archive.entries
                        if e.name is not None
                        and os.path.basename(e.name.replace("\\", "/")) == base]
    if len(basename_matches) == 1:
        return basename_matches[0]
    if len(basename_matches) > 1:
        raise XP3Error("ambiguous",
                       "基线名 %r 匹配多个条目，请用完整路径或哈希"
                       % base)
    raise XP3Error("not-found",
                   "未找到条目 %r（可用哈希 8 位 hex、完整路径或基线名）"
                   % selector)


def safe_relpath(name):
    """把条目名转换为安全相对路径，拒绝路径穿越。"""
    rel = name.replace("\\", "/").lstrip("/")
    if rel.startswith("/") or rel == "":
        raise XP3Error("unsafe-name", "条目名不可作为路径：%r" % name)
    parts = rel.split("/")
    if any(p in ("", ".", "..") for p in parts):
        raise XP3Error("unsafe-name", "条目名包含危险路径成分：%r" % name)
    return rel


# ---------------------------------------------------------------------------
# 输出
# ---------------------------------------------------------------------------

def format_entry_line(e, show_hash=True):
    size = "org=%d arc=%d" % (e.org_size, e.arc_size)
    seg = "segs=%d" % len(e.segments)
    extra = []
    if e.protected:
        extra.append("protected")
    if any(s.compressed for s in e.segments):
        extra.append("zlib")
    tag = ("[%s] " % ",".join(extra)) if extra else ""
    if show_hash and e.path_hash is not None:
        return "0x%08x  %-44s %-22s %-8s %s" % (
            e.path_hash, e.display_name, size, seg, tag)
    return "%-48s %-22s %-8s %s" % (e.display_name, size, seg, tag)


# ---------------------------------------------------------------------------
# 子命令
# ---------------------------------------------------------------------------

def cmd_list(args):
    arc = XP3Archive(args.archive).parse()
    for e in arc.entries:
        print(format_entry_line(e))
    print("-- %d file(s), index=%s, magic=%s --"
          % (len(arc.entries),
             "zlib" if (arc.index_flag & INDEX_ENCODE_METHOD_MASK)
             == INDEX_ENCODE_ZLIB else "raw",
             arc.magic_used))
    if args.json:
        import json
        print(json.dumps([{
            "hash": "0x%08x" % e.path_hash if e.path_hash is not None else None,
            "name": e.name,
            "original_size": e.org_size,
            "archived_size": e.arc_size,
            "segments": len(e.segments),
            "compressed": any(s.compressed for s in e.segments),
            "protected": e.protected,
        } for e in arc.entries], indent=2))


def cmd_extract(args):
    arc = XP3Archive(args.archive).parse()
    entry = select_entry(arc, args.target)
    out_dir = args.out or os.getcwd()
    os.makedirs(out_dir, exist_ok=True)

    if entry.name:
        rel = safe_relpath(entry.name)
        out_path = os.path.join(out_dir, rel)
        os.makedirs(os.path.dirname(out_path) or out_dir, exist_ok=True)
    else:
        out_path = os.path.join(out_dir, "0x%08x.bin" % entry.path_hash)

    data = arc.read_entry(entry)
    if os.path.exists(out_path) and not args.force:
        raise XP3Error("exists", "输出文件已存在（用 --force 覆盖）：%s" % out_path)
    with open(out_path, "wb") as f:
        f.write(data)
    if entry.name is None and entry.path_hash is not None:
        # 名字不可用时写 .hash 侧车文件记录寻找信息
        with open(out_path + ".hash", "w", encoding="utf-8") as f:
            f.write("0x%08x\n" % entry.path_hash)
    print("extracted %s (%d bytes) -> %s"
          % (entry.display_name, len(data), out_path))


def cmd_extract_all(args):
    arc = XP3Archive(args.archive).parse()
    out_dir = args.out or os.getcwd()
    os.makedirs(out_dir, exist_ok=True)
    manifest = []
    count = 0
    total_bytes = 0
    for e in arc.entries:
        data = arc.read_entry(e)
        if e.name and args.use_names:
            rel = safe_relpath(e.name)
            out_path = os.path.join(out_dir, rel)
            os.makedirs(os.path.dirname(out_path) or out_dir, exist_ok=True)
            label = e.name
        else:
            # v1 默认：以 8 位哈希名落盘（避免名字缺失/冲突）
            ext = ""
            if e.name:
                base = os.path.basename(e.name.replace("\\", "/"))
                if "." in base:
                    ext = base.rsplit(".", 1)[1][:8] or ""
            out_path = os.path.join(
                out_dir, "0x%08x%s" % (e.path_hash or count, ("." + ext) if ext else ""))
            if os.path.isfile(out_path) and not args.force:
                raise XP3Error("exists",
                               "输出文件已存在（用 --force 覆盖）：%s" % out_path)
            label = "0x%08x" % (e.path_hash or count)
        with open(out_path, "wb") as f:
            f.write(data)
        manifest.append((label, e.org_size, out_path))
        count += 1
        total_bytes += len(data)
    with open(os.path.join(out_dir, "manifest.txt"), "w", encoding="utf-8") as f:
        for label, size, path in manifest:
            f.write("%-48s %10d  %s\n" % (label, size, os.path.relpath(path, out_dir)))
    print("extracted %d file(s), %d bytes total -> %s"
          % (count, total_bytes, out_dir))
    print("manifest: %s" % os.path.join(out_dir, "manifest.txt"))


def cmd_verify(args):
    arc = XP3Archive(args.archive).parse()
    stats = arc.verify()
    print("OK: %(files)d file(s), %(total_original_bytes)d original bytes, "
          "%(total_archived_bytes)d archived bytes, "
          "%(compressed_files)d compressed, index=%(index_encode)s"
          % stats)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def build_parser():
    p = argparse.ArgumentParser(
        prog="xp3_tool.py",
        description="KiriKiri .xp3 归档只读解析器（v1：未加密索引 + zlib/raw 数据段）",
    )
    sub = p.add_subparsers(dest="command", required=True)

    pl = sub.add_parser("list", help="列出归档内文件")
    pl.add_argument("archive")
    pl.add_argument("--json", action="store_true", help="输出 JSON")

    pe = sub.add_parser("extract", help="解压单个文件（按哈希或名字）")
    pe.add_argument("archive")
    pe.add_argument("target", help="8 位十六进制哈希（可前缀）或文件名")
    pe.add_argument("--out", default=None, help="输出目录（默认当前目录）")
    pe.add_argument("--force", action="store_true", help="覆盖已存在文件")

    pa = sub.add_parser("extract-all", help="解压全部文件（v1 默认哈希名落盘）")
    pa.add_argument("archive")
    pa.add_argument("--out", default=None, help="输出目录（默认当前目录）")
    pa.add_argument("--use-names", action="store_true",
                    help="用归档内真实路径名落盘（默认用哈希名，v1 约定）")
    pa.add_argument("--force", action="store_true", help="覆盖已存在文件")

    pv = sub.add_parser("verify", help="校验归档结构并试解压全部数据")
    pv.add_argument("archive")

    return p


def main(argv=None):
    args = build_parser().parse_args(argv)
    try:
        if args.command == "list":
            cmd_list(args)
        elif args.command == "extract":
            cmd_extract(args)
        elif args.command == "extract-all":
            cmd_extract_all(args)
        elif args.command == "verify":
            cmd_verify(args)
    except XP3Error as e:
        print("xp3_tool: error[%s]: %s" % (e.kind, e.message), file=sys.stderr)
        return 2
    except OSError as e:
        print("xp3_tool: io error: %s" % e, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
