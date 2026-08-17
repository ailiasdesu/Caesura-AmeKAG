#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_xp3_tool.py - tools/xp3_tool.py 的合成样例测试（unittest，零外部依赖）
=============================================================================

按 KiriKiri 规范魔数手工打包 .xp3（未加密索引），验证：
  * list / extract / extract-all / verify 与手工数据往返一致
  * 损坏魔数被拒绝（bad-magic）
  * 加密索引标记报清晰错误（encrypted-index）
  * 损坏索引（zlib 垃圾）报错（index-zlib-error）
  * 链式索引（INDEX_CONTINUE）报不支持错误

运行方式（项目根目录）：
  python tests/scripts/test_xp3_tool.py
也可被 pytest 收集（类/方法命名按约定）。
"""

import os
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

# 让 tools/ 可导入
_REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))
_TOOLS_DIR = os.path.join(_REPO_ROOT, "tools")
if _TOOLS_DIR not in sys.path:
    sys.path.insert(0, _TOOLS_DIR)

import xp3_tool  # noqa: E402


# ---------------------------------------------------------------------------
# 手工打包器（严格对照 krkrz base/XP3Archive.h 布局）
# ---------------------------------------------------------------------------

XP3_MAGIC = b"XP3\r\n \n\x1a\x8bg\x01"
INDEX_RAW, INDEX_ZLIB = 0, 1
FILE_PROTECTED = 1 << 31
INDEX_CONTINUE = 0x80


def _chunk(tag, payload):
    return tag + struct.pack("<Q", len(payload)) + payload


def _info_chunk(flags, org_size, arc_size, name):
    enc = name.encode("utf-16-le")
    payload = (struct.pack("<I", flags) + struct.pack("<Q", org_size)
               + struct.pack("<Q", arc_size) + struct.pack("<h", len(name))
               + enc)
    return _chunk(b"info", payload)


def _segm_chunk(segments):
    """segments: list of (compressed: bool, start: int, org: int, arc: int)"""
    payload = b""
    for compressed, start, org, arc in segments:
        seg_flags = INDEX_ZLIB if compressed else INDEX_RAW
        payload += (struct.pack("<I", seg_flags) + struct.pack("<Q", start)
                    + struct.pack("<Q", org) + struct.pack("<Q", arc))
    return _chunk(b"segm", payload)


def _adlr_chunk(path_hash):
    return _chunk(b"adlr", struct.pack("<I", path_hash))


def build_index_payload(entries):
    """entries: list of dict(name, path_hash, flags, segments)"""
    payload = b""
    for e in entries:
        segs = e["segments"]  # list[(compressed, start, org, arc)]
        org = sum(s[2] for s in segs)
        arc = sum(s[3] for s in segs)
        file_body = (_info_chunk(e.get("flags", 0), org, arc, e["name"])
                     + _segm_chunk(segs) + _adlr_chunk(e["path_hash"]))
        payload += _chunk(b"File", file_body)
    return payload


def write_index_block(f, index_payload, index_flag):
    f.write(bytes([index_flag]))
    if index_flag & 0x07 == INDEX_ZLIB:
        comp = zlib.compress(index_payload)
        f.write(struct.pack("<Q", len(comp)))
        f.write(struct.pack("<Q", len(index_payload)))
        f.write(comp)
    else:
        f.write(struct.pack("<Q", len(index_payload)))
        f.write(index_payload)


def build_xp3(path, entries, index_flag=INDEX_ZLIB):
    """手工打包一个未加密索引 .xp3。

    entries: list of dict(
        name: str,
        path_hash: int,
        data: bytes,                        # 单段内容
        segments: Optional[list[(compressed, data)]],  # 多段；与 data 互斥
        compress: Optional[bool] = True,    # 单段是否压缩
        flags: Optional[int] = 0,
    )
    索引写在整个数据段之后。返回 (每文件记录, index_ofs)。
    """
    records = []
    with open(path, "wb") as f:
        f.write(XP3_MAGIC)
        f.write(struct.pack("<Q", 0))  # 索引偏移占位，稍后回填
        seg_specs = []                  # (name, path_hash, flags, list[(comp,start,org,arc)])
        for e in entries:
            if "segments" in e:
                seg_parts = e["segments"]
            else:
                seg_parts = [(e.get("compress", True), e["data"])]
            segs = []
            for compressed, data in seg_parts:
                start = f.tell()
                if compressed:
                    f.write(zlib.compress(data))
                    arc = f.tell() - start
                else:
                    f.write(data)
                    arc = len(data)
                segs.append((compressed, start, len(data), arc))
            seg_specs.append((e["name"], e["path_hash"],
                              e.get("flags", 0), segs))
            records.append({"name": e["name"], "data": e.get("data"),
                            "path_hash": e["path_hash"]})

        index_ofs = f.tell()
        index_payload = build_index_payload([
            {"name": n, "path_hash": h, "flags": fl, "segments": segs}
            for n, h, fl, segs in seg_specs
        ])
        write_index_block(f, index_payload, index_flag)

        f.seek(len(XP3_MAGIC))
        f.write(struct.pack("<Q", index_ofs))
    return records, index_ofs


def build_corrupt_magic(path):
    with open(path, "wb") as f:
        f.write(b"XPK3\r\n \n\x1a\x8bg\x01")  # 'P' -> 'K'
        f.write(struct.pack("<Q", 0x200))


def build_encrypted_index_marker(path):
    """索引位置处 u32 == 0x80（GARbro 加密/重定向标记）。"""
    with open(path, "wb") as f:
        f.write(XP3_MAGIC)
        f.write(struct.pack("<Q", 0x200))
        f.seek(0x200)
        f.write(struct.pack("<I", 0x80))      # 加密标记
        f.write(struct.pack("<Q", 0x300))     # 假重定向偏移
        f.seek(0x300)
        f.write(bytes([0x04]))                # 未知方法，不应读到


def build_continue_index(path):
    """flag 带 INDEX_CONTINUE 的链式索引。"""
    with open(path, "wb") as f:
        f.write(XP3_MAGIC)
        f.write(struct.pack("<Q", 0x200))
        f.seek(0x200)
        f.write(bytes([INDEX_CONTINUE | INDEX_ZLIB]))
        f.write(struct.pack("<Q", 0))  # compressed_size
        f.write(struct.pack("<Q", 0))  # index_size


def build_garbage_zlib_index(path):
    with open(path, "wb") as f:
        f.write(XP3_MAGIC)
        f.write(struct.pack("<Q", 0x200))
        f.seek(0x200)
        f.write(bytes([INDEX_ZLIB]))
        f.write(struct.pack("<Q", 8))
        f.write(struct.pack("<Q", 100))
        f.write(b"\x00NOTZIP\x01")


# ---------------------------------------------------------------------------
# 样例数据
# ---------------------------------------------------------------------------

SAMPLE_FILES = [
    {"name": "bg/op.bmp", "path_hash": 0x11111111,
     "data": b"BM" + bytes(range(256)) * 3, "compress": True},
    {"name": "bg/bg01.tlg", "path_hash": 0x22222222,
     "data": bytes([0x10, 0x00]) + b"\x00" * 64 + os.urandom(128), "compress": True},
    {"name": "voice/\u3042\u304b\u306d\u3001\u5f71.ogg", "path_hash": 0x33333333,
     "data": b"OggS" + b"\x01" * 300, "compress": False},
    {"name": "data/multi.bin", "path_hash": 0x44444444,
     "segments": [(True, b"first-segment-" * 8),
                  (False, b"|second-raw-part\x00\xff")]},
]


def build_sample(dirpath, index_flag=INDEX_ZLIB, files=None):
    path = os.path.join(dirpath, "sample.xp3")
    records, index_ofs = build_xp3(path, files or SAMPLE_FILES, index_flag)
    return path, records


def run_cli(args):
    """以子进程方式运行 xp3_tool.py，返回 (exit_code, stdout, stderr)。"""
    cmd = [sys.executable, os.path.join(_TOOLS_DIR, "xp3_tool.py")] + args
    proc = subprocess.run(cmd, capture_output=True, text=True,
                          cwd=_REPO_ROOT)
    return proc.returncode, proc.stdout, proc.stderr


# ---------------------------------------------------------------------------
# 测试
# ---------------------------------------------------------------------------

class TestParsing(unittest.TestCase):
    """解析层：模块内直接调用。"""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path, self.records = build_sample(self.tmp.name)

    def test_header_magic_accepted(self):
        arc = xp3_tool.XP3Archive(self.path).parse()
        self.assertEqual(arc.magic_used, "canonical")

    def test_variant_magic_accepted(self):
        p = os.path.join(self.tmp.name, "variant.xp3")
        with open(self.path, "rb") as fh:
            data = bytearray(fh.read())
        data[:11] = xp3_tool.XP3_MAGIC_VARIANT
        with open(p, "wb") as f:
            f.write(data)
        arc = xp3_tool.XP3Archive(p).parse()
        self.assertEqual(arc.magic_used, "variant")
        self.assertEqual(len(arc.entries), len(SAMPLE_FILES))

    def test_entry_count_and_names(self):
        arc = xp3_tool.XP3Archive(self.path).parse()
        self.assertEqual(len(arc.entries), len(SAMPLE_FILES))
        names = {e.name for e in arc.entries}
        self.assertEqual(names, {f["name"] for f in SAMPLE_FILES})
        hashes = {e.path_hash for e in arc.entries}
        self.assertEqual(hashes, {f["path_hash"] for f in SAMPLE_FILES})

    def test_roundtrip_data(self):
        arc = xp3_tool.XP3Archive(self.path).parse()
        for e in arc.entries:
            data = arc.read_entry(e)
            rec = next(r for r in self.records if r["name"] == e.name)
            if rec["data"] is not None:
                self.assertEqual(data, rec["data"])
            else:
                src = next(f for f in SAMPLE_FILES if f["name"] == e.name)
                expect = b"".join(d for _, d in src["segments"])
                self.assertEqual(data, expect)

    def test_multi_segment_stitch(self):
        arc = xp3_tool.XP3Archive(self.path).parse()
        multi = next(e for e in arc.entries if e.name == "data/multi.bin")
        self.assertEqual(len(multi.segments), 2)
        src = next(f for f in SAMPLE_FILES if f["name"] == multi.name)
        self.assertEqual(arc.read_entry(multi),
                         b"".join(d for _, d in src["segments"]))

    def test_raw_index_variant(self):
        p = os.path.join(self.tmp.name, "rawidx.xp3")
        build_xp3(p, SAMPLE_FILES, index_flag=INDEX_RAW)
        arc = xp3_tool.XP3Archive(p).parse()
        self.assertEqual(arc.index_flag & 0x07, INDEX_RAW)
        for e in arc.entries:
            data = arc.read_entry(e)
            self.assertEqual(len(data), e.org_size)

    def test_verify_stats(self):
        arc = xp3_tool.XP3Archive(self.path).parse()
        stats = arc.verify()
        self.assertEqual(stats["files"], len(SAMPLE_FILES))
        self.assertTrue(stats["total_original_bytes"] > 0)

    def test_select_by_hash_prefix(self):
        arc = xp3_tool.XP3Archive(self.path).parse()
        e = xp3_tool.select_entry(arc, "1111")
        self.assertEqual(e.path_hash, 0x11111111)
        e = xp3_tool.select_entry(arc, "0x22222222")
        self.assertEqual(e.path_hash, 0x22222222)

    def test_select_by_name_and_basename(self):
        arc = xp3_tool.XP3Archive(self.path).parse()
        e = xp3_tool.select_entry(arc, "bg/bg01.tlg")
        self.assertEqual(e.name, "bg/bg01.tlg")
        e = xp3_tool.select_entry(arc, "bg01.tlg")  # 基线名唯一
        self.assertEqual(e.name, "bg/bg01.tlg")

    def test_not_found(self):
        arc = xp3_tool.XP3Archive(self.path).parse()
        with self.assertRaises(xp3_tool.XP3Error) as cm:
            xp3_tool.select_entry(arc, "nope/never.xyz")
        self.assertEqual(cm.exception.kind, "not-found")

    def test_protected_flag_recorded(self):
        p = os.path.join(self.tmp.name, "prot.xp3")
        files = [dict(SAMPLE_FILES[0], flags=FILE_PROTECTED)]
        build_xp3(p, files)
        arc = xp3_tool.XP3Archive(p).parse()
        self.assertTrue(arc.entries[0].protected)
        self.assertEqual(arc.read_entry(arc.entries[0]),
                         SAMPLE_FILES[0]["data"])


class TestCorruptionDetection(unittest.TestCase):
    """损坏与不支持场景必须报清晰错误。"""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)

    def test_bad_magic_rejected(self):
        p = os.path.join(self.tmp.name, "bad.xp3")
        build_corrupt_magic(p)
        with self.assertRaises(xp3_tool.XP3Error) as cm:
            xp3_tool.XP3Archive(p).parse()
        self.assertEqual(cm.exception.kind, "bad-magic")

    def test_encrypted_index_marker(self):
        p = os.path.join(self.tmp.name, "enc.xp3")
        build_encrypted_index_marker(p)
        with self.assertRaises(xp3_tool.XP3Error) as cm:
            xp3_tool.XP3Archive(p).parse()
        self.assertEqual(cm.exception.kind, "encrypted-index")
        self.assertIn("加密", cm.exception.message)

    def test_unknown_index_encoding(self):
        p = os.path.join(self.tmp.name, "unk.xp3")
        with open(p, "wb") as f:
            f.write(XP3_MAGIC)
            f.write(struct.pack("<Q", 0x200))
            f.seek(0x200)
            f.write(bytes([0x04]))  # 方法 4：未知/加密
        with self.assertRaises(xp3_tool.XP3Error) as cm:
            xp3_tool.XP3Archive(p).parse()
        self.assertEqual(cm.exception.kind, "encrypted-index")

    def test_continue_index_unsupported(self):
        p = os.path.join(self.tmp.name, "cont.xp3")
        build_continue_index(p)
        with self.assertRaises(xp3_tool.XP3Error) as cm:
            xp3_tool.XP3Archive(p).parse()
        self.assertEqual(cm.exception.kind, "index-continue-unsupported")

    def test_garbage_zlib_index(self):
        p = os.path.join(self.tmp.name, "gzbad.xp3")
        build_garbage_zlib_index(p)
        with self.assertRaises(xp3_tool.XP3Error) as cm:
            xp3_tool.XP3Archive(p).parse()
        self.assertEqual(cm.exception.kind, "index-zlib-error")

    def test_truncated_header(self):
        p = os.path.join(self.tmp.name, "trunc.xp3")
        with open(p, "wb") as f:
            f.write(XP3_MAGIC)  # 只有 11 字节
        with self.assertRaises(xp3_tool.XP3Error) as cm:
            xp3_tool.XP3Archive(p).parse()
        self.assertEqual(cm.exception.kind, "truncated-header")


class TestCli(unittest.TestCase):
    """CLI 层：子进程往返。"""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path, self.records = build_sample(self.tmp.name)
        self.out = os.path.join(self.tmp.name, "out")
        os.makedirs(self.out, exist_ok=True)

    def test_cli_list(self):
        code, out, err = run_cli(["list", self.path])
        self.assertEqual(code, 0, err)
        for f in SAMPLE_FILES:
            self.assertIn(f["name"], out)
        self.assertIn("0x11111111", out)
        self.assertIn("4 file(s)", out)

    def test_cli_extract_by_hash(self):
        code, out, err = run_cli(
            ["extract", self.path, "0x22222222", "--out", self.out])
        self.assertEqual(code, 0, err)
        fname = os.path.join(self.out, "bg", "bg01.tlg")
        self.assertTrue(os.path.isfile(fname))
        with open(fname, "rb") as f:
            self.assertEqual(f.read(),
                             next(x for x in SAMPLE_FILES
                                  if x["path_hash"] == 0x22222222)["data"])

    def test_cli_extract_by_name(self):
        code, out, err = run_cli(
            ["extract", self.path, "bg/op.bmp", "--out", self.out])
        self.assertEqual(code, 0, err)
        fname = os.path.join(self.out, "bg", "op.bmp")
        self.assertTrue(os.path.isfile(fname))
        with open(fname, "rb") as f:
            self.assertEqual(f.read(),
                             next(x for x in SAMPLE_FILES
                                  if x["name"] == "bg/op.bmp")["data"])

    def test_cli_extract_unicode_name(self):
        code, out, err = run_cli(
            ["extract", self.path,
             "voice/\u3042\u304b\u306d\u3001\u5f71.ogg", "--out", self.out])
        self.assertEqual(code, 0, err)
        files = os.listdir(os.path.join(self.out, "voice"))
        self.assertEqual(len(files), 1)

    def test_cli_extract_all_hash_names(self):
        code, out, err = run_cli(
            ["extract-all", self.path, "--out", self.out])
        self.assertEqual(code, 0, err)
        self.assertTrue(os.path.isfile(os.path.join(self.out, "manifest.txt")))
        self.assertTrue(os.path.isfile(
            os.path.join(self.out, "0x11111111.bmp")))   # 扩展名保留
        self.assertTrue(os.path.isfile(
            os.path.join(self.out, "0x22222222.tlg")))
        self.assertTrue(os.path.isfile(
            os.path.join(self.out, "0x44444444.bin")))   # 扩展名保留
        for name in ("0x11111111.bmp", "0x22222222.tlg"):
            with open(os.path.join(self.out, name), "rb") as f:
                self.assertGreater(len(f.read()), 0)

    def test_cli_extract_all_use_names(self):
        out2 = os.path.join(self.tmp.name, "out2")
        code, out, err = run_cli(
            ["extract-all", self.path, "--out", out2, "--use-names"])
        self.assertEqual(code, 0, err)
        for rel in ("bg/op.bmp", "bg/bg01.tlg", "data/multi.bin"):
            self.assertTrue(os.path.isfile(os.path.join(out2, rel)), rel)

    def test_cli_verify(self):
        code, out, err = run_cli(["verify", self.path])
        self.assertEqual(code, 0, err)
        self.assertIn("OK:", out)
        self.assertIn("4 file(s)", out)

    def test_cli_bad_magic_rejected(self):
        bad = os.path.join(self.tmp.name, "bad.xp3")
        build_corrupt_magic(bad)
        code, out, err = run_cli(["list", bad])
        self.assertEqual(code, 2)
        self.assertIn("bad-magic", err)

    def test_cli_encrypted_index_error(self):
        enc = os.path.join(self.tmp.name, "enc.xp3")
        build_encrypted_index_marker(enc)
        code, out, err = run_cli(["list", enc])
        self.assertEqual(code, 2)
        self.assertIn("encrypted-index", err)
        self.assertIn("加密", err)

    def test_cli_extract_not_found(self):
        code, out, err = run_cli(
            ["extract", self.path, "0xdeadbeef", "--out", self.out])
        self.assertEqual(code, 2)
        self.assertIn("not-found", err)


if __name__ == "__main__":
    unittest.main(verbosity=2)
