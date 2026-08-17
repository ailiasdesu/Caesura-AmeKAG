#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
tlg2png.py - KiriKiri (吉里吉里) .tlg 图像解码器（TLG5 / TLG6 → PNG）
======================================================================

独立 CLI 工具，把 KiriKiri 引擎的专有图像格式 .tlg 解码为 PNG。
算法参照 krkrz 官方实现（visual/LoadTLG.cpp）与 GARbro 移植
（ArcFormats/KiriKiri/ImageTLG.cs）逐行对齐。

TLG5 (v1 支持)
--------------
* header "TLG5.0\0raw\x1a" → colors(3|4) / width / height / blockheight
* 分块（block）压缩：每块每个颜色平面一个数据段
* 数据段两种编码：mark==0 -> 修正 LZSS（12bit 窗口索引 + 4bit 长度 + 扩展字节）
  滑动窗口 4096 字节、跨颜色平面/块连续；mark!=0 -> 原始字节
* 颜色差分重组：B+G / G / R+G 及逐行水平累加 + 上邻行垂直差分（mod 256）

TLG6 (v1 支持)
--------------
* header "TLG6.0\0raw\x1a" → colors(1|3|4) / flags / width / height /
  max_bit_length，随后是压缩的 8x8 块滤波类型表（LZSS + 预置 4096 字节窗口）
* 每 8 行一组、每通道一个 Golomb(修正 Rice/MRM) 比特流：
  首 bit 为"首段是否全零"标志，段长为 unary 计数 + 二进制后缀；
  非零值用 4 状态自适应 Golomb 表 + 符号-幅度编码
* 按 8x8 块应用 32 种色度/亮度滤波预测（MED/AVG 及通道线性组合），
  行间以 zigzag 方向扫描（偶行左->右，奇行右->左）

v1 明确不支持（报清晰错误）
---------------------------
* TLG6 熵编码方法非 00（Golomb）：01 Gamma / 10 修正 LZSS / 11 原始 —— 未实现
* 变体怪异头（TLG0.0\0sds\x1a 旧式 15 字节前缀以下兼容；XXXYYY/JKMXE8 等
  被 XOR 混淆的变体不支持）
* TLG6 非零 data flag / color type / external golomb table 标志
* 引擎侧多图合成（TLG 尾部 "tags" 元数据引用基底图）——本工具只给出解码像素
* TLG5 的 universal transition rule（glm 非 normal 模式）

用法
----
  python tools/tlg2png.py <input.tlg> [output.png] [--info] [--via-pillow]

  --info        只打印头信息与解码统计，不写 PNG
  --via-pillow  用 Pillow 写 PNG（默认用 zlib+struct 手写，零依赖更稳）
  未指定输出名时自动把扩展名换成 .png
"""
import argparse
import struct
import sys
import zlib

# ---------------------------------------------------------------------------
# 常量
# ---------------------------------------------------------------------------

TLG5_SIG = b"TLG5.0\x00raw\x1a"
TLG6_SIG = b"TLG6.0\x00raw\x1a"
LEGACY_PREFIX = b"TLG0.0\x00sds\x1a"   # 旧式文件在此前缀后第 15 字节才是签名
OBFUSCATED_MARKS = (b"XXXYYY", b"XXXZZZ", b"JKMXE8")  # XOR 混淆变体标记

LZSS_WINDOW_SIZE = 4096

# TLG6 8x8 滤波块
T6_W_BLOCK = 8
T6_H_BLOCK = 8
T6_GOLOMB_N_COUNT = 4
T6_LZ_BITS = 12
T6_LZ_SIZE = 1 << T6_LZ_BITS

# 自适应 Golomb 表分布（W.Dee 调参；总和必须为 4*2*128 = 1024）
T6_GOLOMB_COMPRESSED = (
    (3, 7, 15, 27, 63, 108, 223, 448, 130),
    (3, 5, 13, 24, 51, 95, 192, 384, 257),
    (2, 5, 12, 21, 39, 86, 155, 320, 384),
    (2, 3, 9, 18, 33, 61, 129, 258, 511),
)

U32MASK = 0xFFFFFFFF


class TLGError(Exception):
    """格式错误/不支持的变体。message 面向用户。"""


# ---------------------------------------------------------------------------
# 基础工具
# ---------------------------------------------------------------------------

def u32_le(b, off):
    if off + 4 > len(b):
        raise TLGError("文件截断：读取 u32 超出数据末尾 (offset=%d)" % off)
    return struct.unpack_from("<I", b, off)[0]


def build_leading_zero_table():
    """与 krkrz TVPTLG6InitLeadingZeroTable 等价：返回最小置位位索引+1，0 输入为 0。"""
    return bytes((0 if i == 0 else (i & -i).bit_length()) for i in range(4096))


def build_golomb_bitlen_table():
    """TVPTLG6GolombBitLengthTable[a][n]：按压缩分布展开成 1024 项。"""
    tbl = [[0] * T6_GOLOMB_N_COUNT for _ in range(4 * 2 * 128)]
    for n in range(T6_GOLOMB_N_COUNT):
        a = 0
        for i in range(9):
            for _ in range(T6_GOLOMB_COMPRESSED[n][i]):
                tbl[a][n] = i
                a += 1
        if a != len(tbl):
            raise TLGError("内部错误：Golomb 表分布校验失败")
    return tbl


# LZSS / 压缩 ----------------------------------------------------------------

def decompress_slide(outbuf, inbuf, window, r):
    """修正 LZSS 解压（krkrz TVPTLG5DecompressSlide 逐行等价）。

    outbuf: 输出缓冲（bytearray/mutable），从下标 0 开始连续写
    window: 4096 字节环形滑动窗口（调用间保持，跨平面/块延续）
    r     : 窗口写指针（调用间延续）
    返回新的 r。
    """
    o = 0
    flags = 0
    i = 0
    n = len(inbuf)
    wmask = len(window) - 1
    olen = len(outbuf)
    while i < n:
        flags >>= 1
        if (flags & 256) == 0:
            if i >= n:
                raise TLGError("LZSS 数据截断（控制标志）")
            flags = inbuf[i] | 0xFF00
            i += 1
        if flags & 1:
            if i + 2 > n:
                raise TLGError("LZSS 数据截断（匹配令牌）")
            mpos = inbuf[i] | ((inbuf[i + 1] & 0x0F) << 8)
            mlen = (inbuf[i + 1] & 0xF0) >> 4
            i += 2
            mlen += 3
            if mlen == 18:  # 18 -> 长度扩展：再读一个字节
                if i >= n:
                    raise TLGError("LZSS 数据截断（长度扩展）")
                mlen += inbuf[i]
                i += 1
            if o + mlen > olen:
                raise TLGError("LZSS 输出溢出（目标缓冲不够）")
            while mlen:
                b = window[mpos]
                outbuf[o] = b
                o += 1
                window[r] = b
                mpos = (mpos + 1) & wmask
                r = (r + 1) & wmask
                mlen -= 1
        else:
            if i >= n:
                raise TLGError("LZSS 数据截断（字面量）")
            c = inbuf[i]
            i += 1
            if o >= olen:
                raise TLGError("LZSS 输出溢出（目标缓冲不够）")
            outbuf[o] = c
            o += 1
            window[r] = c
            r = (r + 1) & wmask
    return r


def build_tlg6_preset_window():
    """TLG6 滤波类型流使用的预置 4096 字节窗口（krkrz 同款初始化）。"""
    w = bytearray(LZSS_WINDOW_SIZE)
    p = 0
    for i in range(32):
        ib = bytes((i, i, i, i))
        for j in range(16):
            jb = bytes((j, j, j, j))
            w[p:p + 8] = ib + jb
            p += 8
    return w


# ---------------------------------------------------------------------------
# TLG6 位流（逐字对齐 C# 的 u32 小端窗口读取）
# ---------------------------------------------------------------------------

class _BitReader:
    __slots__ = ("pool", "idx", "pos")

    def __init__(self, pool):
        # 尾部补 64 字节零，保证越界 u32 读取安全（转义路径会跳跃读取）
        self.pool = pool + bytes(64)
        self.idx = 0
        self.pos = 1  # 第 0 位是"首段是否全零"标志，已在外面消费

    def u32(self):
        return struct.unpack_from("<I", self.pool, self.idx)[0] >> self.pos

    def read_up_to(self):
        """读取 unary 计数 → 二进制后缀，返回 run 长度（正整数）。"""
        v = self.u32()
        bit_count = 0
        b = _LZ_TABLE[v & (T6_LZ_SIZE - 1)]
        bit_count = b
        while b == 0:
            bit_count += T6_LZ_BITS
            self.advance(T6_LZ_BITS)
            v = self.u32()
            b = _LZ_TABLE[v & (T6_LZ_SIZE - 1)]
            bit_count += b
        # 这里的 b 包含"找到的 1"；bit_count-1 是跳过的 0 数
        self.advance(b)
        zeros = bit_count - 1
        count = (1 << zeros) + (self.u32() & ((1 << zeros) - 1))
        self.advance(zeros)
        return count

    def advance(self, bits):
        self.pos += bits
        self.idx += self.pos >> 3
        self.pos &= 7


def _decode_golomb_channel(pool, offset_dword, pixel_count):
    """单通道 Golomb 解码，返回 list[字节值]。

    offset_dword: 通道在 32 位像素槽中的字节位移（0/8/16/24）。
    等价于 GARbro TVPTLG6DecodeGolombValues(ForFirst)。
    """
    if pixel_count <= 0:
        return []
    if len(pool) == 0:
        raise TLGError("TLG6 通道比特流为空但存在像素")
    mask = ~(0xFF << offset_dword) & U32MASK
    r = _BitReader(pool)
    zero = (pool[0] & 1) == 0  # bit0：首段是否全零
    out = [0] * pixel_count
    a = 0
    n = T6_GOLOMB_N_COUNT - 1
    pixel = 0
    while pixel < pixel_count:
        count = r.read_up_to()
        if zero:
            for _ in range(count):
                if pixel >= pixel_count:
                    break
                out[pixel] = 0
                pixel += 1
            zero = False
        else:
            for _ in range(count):
                if pixel >= pixel_count:
                    break
                k = _GB_TABLE[a][n]
                v = r.u32()
                if v:
                    b = _LZ_TABLE[v & (T6_LZ_SIZE - 1)]
                    bit_count = b
                    while b == 0:
                        bit_count += T6_LZ_BITS
                        r.advance(T6_LZ_BITS)
                        v = r.u32()
                        b = _LZ_TABLE[v & (T6_LZ_SIZE - 1)]
                        bit_count += b
                    bit_count -= 1
                else:  # 当前 32 位窗口全零：转义字节直接给出 leading-zero 计数
                    r.idx += 5
                    bit_count = r.pool[r.idx - 1]
                    r.pos = 0
                    v = r.u32()
                    b = 0
                val = (bit_count << k) + ((v >> b) & ((1 << k) - 1))
                sign = (val & 1) - 1
                val >>= 1
                a += abs(val)
                if a >= 1024:
                    a = 1023
                byte_val = ((val ^ sign) + sign + 1) & 0xFF
                out[pixel] = byte_val
                r.advance(b + k)
                pixel += 1
                n -= 1
                if n < 0:
                    a >>= 1
                    n = T6_GOLOMB_N_COUNT - 1
            zero = True
    return out


# TLG6 像素组装（MED/AVG 与 32 种滤波）--------------------------------------

def _make_gt_mask(a, b):
    tmp2 = (~b) & U32MASK
    tmp = ((a & tmp2) + (((a ^ tmp2) >> 1) & 0x7F7F7F7F)) & 0x80808080
    return (((tmp >> 7) + 0x7F7F7F7F) ^ 0x7F7F7F7F) & U32MASK


def _packed_add(a, b):
    tmp = (((a & b) << 1) + ((a ^ b) & 0xFEFEFEFE)) & 0x01010100
    return (a + b - tmp) & U32MASK


def _med2(a, b, c):
    """按字节独立的 3 值中值。"""
    agt = _make_gt_mask(a, b)
    axb = (a ^ b) & agt
    aa = axb ^ a
    bb = axb ^ b
    n = _make_gt_mask(c, bb)
    nn = _make_gt_mask(aa, c)
    m = (~(n | nn)) & U32MASK
    return ((n & aa) | (nn & bb) | ((bb & m) - (c & m) + (aa & m))) & U32MASK


def _med(a, b, c, v):
    return _packed_add(_med2(a, b, c), v & U32MASK)


def _avg(a, b, c, v):
    pred = ((a & b) + (((a ^ b) & 0xFEFEFEFE) >> 1) + ((a ^ b) & 0x01010101)) & U32MASK
    return _packed_add(pred, v & U32MASK)


def _t(expr16, expr8, expr0):
    """构造 TLG6 滤波的通道线性变换 v' = f(v)（各通道独立，mod 256）。"""

    def f(v):
        return ((expr16(v) & 0xFF) << 16) | ((expr8(v) & 0xFF) << 8)             | (expr0(v) & 0xFF) | (v & 0xFF000000)
    return f


# 偶数索引 = MED，奇数索引 = AVG。变换定义见 GARbro case 0..31 逐字。
_T6_TRANSFORMS = {
    0:  None,  # 恒等
    2:  _t(lambda v: ((v >> 16) & 0xFF) + ((v >> 8) & 0xFF),
           lambda v: (v >> 8) & 0xFF,
           lambda v: (v & 0xFF) + ((v >> 8) & 0xFF)),
    4:  _t(lambda v: ((v >> 16) & 0xFF) + (v & 0xFF) + ((v >> 8) & 0xFF),
           lambda v: ((v >> 8) & 0xFF) + (v & 0xFF),
           lambda v: v & 0xFF),
    6:  _t(lambda v: (v >> 16) & 0xFF,
           lambda v: ((v >> 8) & 0xFF) + ((v >> 16) & 0xFF),
           lambda v: (v & 0xFF) + ((v >> 16) & 0xFF) + ((v >> 8) & 0xFF)),
    8:  _t(lambda v: ((v >> 16) & 0xFF) + (v & 0xFF) + ((v >> 16) & 0xFF) + ((v >> 8) & 0xFF),
           lambda v: ((v >> 8) & 0xFF) + (v & 0xFF) + ((v >> 16) & 0xFF),
           lambda v: (v & 0xFF) + ((v >> 16) & 0xFF)),
    10: _t(lambda v: (v >> 16) & 0xFF,
           lambda v: ((v >> 8) & 0xFF) + (v & 0xFF) + ((v >> 16) & 0xFF),
           lambda v: (v & 0xFF) + ((v >> 16) & 0xFF)),
    12: _t(lambda v: (v >> 16) & 0xFF,
           lambda v: (v >> 8) & 0xFF,
           lambda v: (v & 0xFF) + ((v >> 8) & 0xFF)),
    14: _t(lambda v: (v >> 16) & 0xFF,
           lambda v: ((v >> 8) & 0xFF) + (v & 0xFF),
           lambda v: v & 0xFF),
    16: _t(lambda v: ((v >> 16) & 0xFF) + ((v >> 8) & 0xFF),
           lambda v: (v >> 8) & 0xFF,
           lambda v: v & 0xFF),
    18: _t(lambda v: ((v >> 16) & 0xFF) + (v & 0xFF),
           lambda v: ((v >> 8) & 0xFF) + ((v >> 16) & 0xFF) + (v & 0xFF),
           lambda v: (v & 0xFF) + ((v >> 8) & 0xFF) + ((v >> 16) & 0xFF) + (v & 0xFF)),
    20: _t(lambda v: (v >> 16) & 0xFF,
           lambda v: ((v >> 8) & 0xFF) + ((v >> 16) & 0xFF),
           lambda v: (v & 0xFF) + ((v >> 16) & 0xFF)),
    22: _t(lambda v: ((v >> 16) & 0xFF) + (v & 0xFF),
           lambda v: ((v >> 8) & 0xFF) + (v & 0xFF),
           lambda v: v & 0xFF),
    24: _t(lambda v: ((v >> 16) & 0xFF) + (v & 0xFF),
           lambda v: ((v >> 8) & 0xFF) + ((v >> 16) & 0xFF) + (v & 0xFF),
           lambda v: v & 0xFF),
    26: _t(lambda v: ((v >> 16) & 0xFF) + (v & 0xFF) + ((v >> 8) & 0xFF),
           lambda v: ((v >> 8) & 0xFF) + ((v >> 16) & 0xFF) + (v & 0xFF) + ((v >> 8) & 0xFF),
           lambda v: (v & 0xFF) + ((v >> 8) & 0xFF)),
    28: _t(lambda v: ((v >> 16) & 0xFF) + (v & 0xFF) + ((v >> 8) & 0xFF) + ((v >> 16) & 0xFF),
           lambda v: ((v >> 8) & 0xFF) + ((v >> 16) & 0xFF),
           lambda v: (v & 0xFF) + ((v >> 8) & 0xFF) + ((v >> 16) & 0xFF)),
    30: _t(lambda v: ((v >> 16) & 0xFF) + ((v & 0xFF) << 1),
           lambda v: ((v >> 8) & 0xFF) + ((v & 0xFF) << 1),
           lambda v: v & 0xFF),
}
# 偶数索引 = MED，奇数索引 = AVG；0/1 为恒等变换（v 不预变换）
FILTERS = []
for i in range(32):
    if i == 0:
        FILTERS.append(_med)
    elif i == 1:
        FILTERS.append(_avg)
    elif i % 2 == 0:
        tf = _T6_TRANSFORMS[i]
        FILTERS.append((lambda t: (lambda a, b, c, v: _med(a, b, c, t(v))))(tf))
    else:
        tf = _T6_TRANSFORMS[i - 1]
        FILTERS.append((lambda t: (lambda a, b, c, v: _avg(a, b, c, t(v))))(tf))

_LZ_TABLE = build_leading_zero_table()
_GB_TABLE = build_golomb_bitlen_table()


def _decode_line_generic(prevline, prevline_index, curline, curline_index,
                         width, start_block, block_limit,
                         filter_types, ft_index, skipbytes,
                         inbuf, inbuf_index, initialp, oddskip, dir_):
    """GARbro TVPTLG6DecodeLineGeneric 逐行等价（像素为 32 位 AARRGGBB 槽）。"""
    if start_block:
        prevline_index += start_block * T6_W_BLOCK
        curline_index += start_block * T6_W_BLOCK
        p = curline[curline_index - 1]
        up = prevline[prevline_index - 1]
    else:
        p = up = initialp

    inbuf_index += skipbytes * start_block
    step = 1 if (dir_ & 1) else -1

    for i in range(start_block, block_limit):
        w = width - i * T6_W_BLOCK
        if w > T6_W_BLOCK:
            w = T6_W_BLOCK
        ww = w
        if step == -1:
            inbuf_index += ww - 1
        if i & 1:
            inbuf_index += oddskip * ww

        decoder = FILTERS[filter_types[ft_index + i]]

        while w:
            u = prevline[prevline_index]
            p = decoder(p, u, up, inbuf[inbuf_index])
            up = u
            curline[curline_index] = p
            curline_index += 1
            prevline_index += 1
            inbuf_index += step
            w -= 1

        if step == 1:
            inbuf_index += skipbytes - ww
        else:
            inbuf_index += skipbytes + 1
        if i & 1:
            inbuf_index -= oddskip * ww


# ---------------------------------------------------------------------------
# 头解析
# ---------------------------------------------------------------------------

def parse_header(data):
    """返回 dict(version, colors, width, height, data_offset, legacy)。"""
    if len(data) < 27:
        raise TLGError("文件过小，不是有效的 TLG 文件")
    base = 0
    legacy = False
    if data[:11] == LEGACY_PREFIX:
        # 旧式：前缀后第 15 字节处才是 "TLG5.0/TLG6.0" 签名
        if data[15:21] in OBFUSCATED_MARKS or data[15:26] in (TLG5_SIG, TLG6_SIG):
            if data[15:21] in OBFUSCATED_MARKS:
                raise TLGError("检测到被 XOR 混淆的 TLG 变体（%r），v1 不支持" % data[15:21])
        else:
            raise TLGError("不支持的旧式 TLG 头（签名位置无 TLG5/TLG6 标记）")
        base = 15
        legacy = True
    elif data[:6] in OBFUSCATED_MARKS:
        raise TLGError("检测到被 XOR 混淆的 TLG 变体（%r），v1 不支持" % data[:6])
    elif data[:11] not in (TLG5_SIG, TLG6_SIG):
        raise TLGError("无效签名字节 %r（需要 TLG5.0\x00raw\x1a 或 TLG6.0\x00raw\x1a）" % data[:11])

    sig = data[base:base + 11]
    version = 6 if sig == TLG6_SIG else 5
    off = base + 11
    colors = data[off]
    off += 1
    if 6 == version:
        if colors not in (1, 3, 4):
            raise TLGError("TLG6 颜色分量数 %d 不支持（合法值 1/3/4）" % colors)
        if data[off] != 0 or data[off + 1] != 0 or data[off + 2] != 0:
            raise TLGError("TLG6 头标志位非零（data flag / color type / external golomb table）"
                           "，v1 不支持")
        off += 3
        width = u32_le(data, off)
        height = u32_le(data, off + 4)
        data_offset = off + 8          # max_bit_length 在此
    else:
        if colors not in (3, 4):
            raise TLGError("TLG5 颜色分量数 %d 不支持（合法值 3/4）" % colors)
        width = u32_le(data, off)
        height = u32_le(data, off + 4)
        data_offset = off + 8          # blockheight 在此
    if width == 0 or height == 0:
        raise TLGError("图像尺寸无效：%dx%d" % (width, height))
    if width * height > 1 << 28:
        raise TLGError("图像过大（%dx%d），拒绝解码" % (width, height))
    return {
        "version": version, "colors": colors, "width": width,
        "height": height, "data_offset": data_offset, "legacy": legacy,
    }


# ---------------------------------------------------------------------------
# TLG5 解码
# ---------------------------------------------------------------------------

def _compose_first_line(out, row0, buf, bufpos, width, colors):
    """首行（无上邻行）：水平差分 + 通道重组，输出 BGRA 到 out[pixel*4..]。"""
    pb = pg = pr = pa = 0
    for x in range(width):
        g = buf[1][bufpos + x]
        b = (buf[0][bufpos + x] + g) & 0xFF
        r = (buf[2][bufpos + x] + g) & 0xFF
        pb = (pb + b) & 0xFF
        pg = (pg + g) & 0xFF
        pr = (pr + r) & 0xFF
        o = (row0 + x) * 4
        out[o] = pb
        out[o + 1] = pg
        out[o + 2] = pr
        if colors == 4:
            pa = (pa + buf[3][bufpos + x]) & 0xFF
            out[o + 3] = pa
        else:
            out[o + 3] = 0xFF


def _compose_line(out, row0, upper_row0, buf, bufpos, width, colors):
    """后续行：水平差分 + 上邻行垂直差分 + 通道重组（mod 256）。"""
    pc0 = pc1 = pc2 = pc3 = 0
    for x in range(width):
        c1 = buf[1][bufpos + x]
        c0 = (buf[0][bufpos + x] + c1) & 0xFF
        c2 = (buf[2][bufpos + x] + c1) & 0xFF
        pc0 = (pc0 + c0) & 0xFF
        pc1 = (pc1 + c1) & 0xFF
        pc2 = (pc2 + c2) & 0xFF
        o = (row0 + x) * 4
        u = (upper_row0 + x) * 4
        out[o] = (pc0 + out[u]) & 0xFF
        out[o + 1] = (pc1 + out[u + 1]) & 0xFF
        out[o + 2] = (pc2 + out[u + 2]) & 0xFF
        if colors == 4:
            c3 = buf[3][bufpos + x]
            pc3 = (pc3 + c3) & 0xFF
            out[o + 3] = (pc3 + out[u + 3]) & 0xFF
        else:
            out[o + 3] = 0xFF


def decode_tlg5(data, header):
    width = header["width"]
    height = header["height"]
    colors = header["colors"]
    off = header["data_offset"]
    blockheight = u32_le(data, off)
    off += 4
    if blockheight <= 0:
        raise TLGError("TLG5 blockheight 必须为正（得到 %d）" % blockheight)
    blockcount = (height - 1) // blockheight + 1
    off += blockcount * 4  # 跳过块大小表（解码器无需使用）

    out = bytearray(width * height * 4)
    window = bytearray(LZSS_WINDOW_SIZE)
    r = 0
    plane = [bytearray(blockheight * width + 16) for _ in range(colors)]
    prevline = -1          # 上一行像素偏移（-1 = 尚无首行）
    for y_blk in range(0, height, blockheight):
        for c in range(colors):
            if off + 5 > len(data):
                raise TLGError("TLG5 数据截断（块内平面头）")
            mark = data[off]
            off += 1
            size = u32_le(data, off)
            off += 4
            if off + size > len(data):
                raise TLGError("TLG5 数据截断（平面数据 size=%d）" % size)
            chunk = data[off:off + size]
            off += size
            if mark == 0:
                r = decompress_slide(plane[c], chunk, window, r)
            else:
                if size > len(plane[c]):
                    raise TLGError("TLG5 原始平面过大")
                plane[c][:size] = chunk
        y_lim = min(y_blk + blockheight, height)
        bufpos = 0          # 每块的平面缓冲从 0 开始
        for y in range(y_blk, y_lim):
            row0 = y * width
            if prevline >= 0:
                _compose_line(out, row0, prevline, plane, bufpos, width, colors)
            else:
                _compose_first_line(out, row0, plane, bufpos, width, colors)
            bufpos += width
            prevline = row0
    return bytes(out)


# ---------------------------------------------------------------------------
# TLG6 解码
# ---------------------------------------------------------------------------

def decode_tlg6(data, header):
    width = header["width"]
    height = header["height"]
    colors = header["colors"]
    off = header["data_offset"]
    max_bit_length = u32_le(data, off)
    off += 4

    x_block_count = (width - 1) // T6_W_BLOCK + 1
    y_block_count = (height - 1) // T6_H_BLOCK + 1
    main_count = width // T6_W_BLOCK
    fraction = width - main_count * T6_W_BLOCK

    image = [0] * (width * height)
    pixelbuf = [0] * (width * T6_H_BLOCK + 1)
    filter_types = bytearray(x_block_count * y_block_count)
    window = build_tlg6_preset_window()

    # 滤波类型表（LZSS 压缩）
    inbuf_size = u32_le(data, off)
    off += 4
    if inbuf_size < 0 or off + inbuf_size > len(data):
        raise TLGError("TLG6 滤波类型流截断")
    decompress_slide(filter_types, data[off:off + inbuf_size], window, 0)
    off += inbuf_size

    zerocolor = 0xFF000000 if colors == 3 else 0
    zeroline = [zerocolor] * width
    prevline = zeroline
    prevline_index = 0

    for y in range(0, height, T6_H_BLOCK):
        ylim = min(y + T6_H_BLOCK, height)
        pixel_count = (ylim - y) * width
        for c in range(colors):
            bit_length_word = u32_le(data, off)
            off += 4
            method = (bit_length_word >> 30) & 3
            nbits = bit_length_word & 0x3FFFFFFF
            if method != 0:
                raise TLGError(
                    "TLG6 不支持的熵编码方法 %d（只有 Golomb(00) 已实现）" % method)
            byte_length = (nbits + 7) // 8
            if off + byte_length > len(data):
                raise TLGError("TLG6 数据截断（比特流 byte_length=%d）" % byte_length)
            pool = data[off:off + byte_length]
            off += byte_length
            chans = _decode_golomb_channel(pool, c * 8, pixel_count)
            shift = c * 8
            for i in range(pixel_count):
                if c == 0:
                    pixelbuf[i] = chans[i]
                else:
                    pixelbuf[i] = (pixelbuf[i] & ~(0xFF << shift) & U32MASK) | (chans[i] << shift)

        ft = (y // T6_H_BLOCK) * x_block_count
        skipbytes = (ylim - y) * T6_W_BLOCK
        for yy in range(y, ylim):
            curline = yy * width
            dir_ = (yy & 1) ^ 1
            oddskip = (ylim - yy - 1) - (yy - y)
            if main_count:
                start = (width if width < T6_W_BLOCK else T6_W_BLOCK) * (yy - y)
                _decode_line_generic(
                    prevline, prevline_index, image, curline, width,
                    0, main_count, filter_types, ft, skipbytes,
                    pixelbuf, start, zerocolor, oddskip, dir_)
            if main_count != x_block_count:
                ww = min(fraction, T6_W_BLOCK)
                start = ww * (yy - y)
                _decode_line_generic(
                    prevline, prevline_index, image, curline, width,
                    main_count, x_block_count, filter_types, ft, skipbytes,
                    pixelbuf, start, zerocolor, oddskip, dir_)
            prevline = image
            prevline_index = curline

    # 32 位 AARRGGBB 槽 → BGRA 字节（对齐 C# Buffer.BlockCopy）
    raw = bytearray(width * height * 4)
    for i, v in enumerate(image):
        raw[i * 4] = v & 0xFF
        raw[i * 4 + 1] = (v >> 8) & 0xFF
        raw[i * 4 + 2] = (v >> 16) & 0xFF
        raw[i * 4 + 3] = (v >> 24) & 0xFF
    return bytes(raw)


# ---------------------------------------------------------------------------
# PNG 输出
# ---------------------------------------------------------------------------

def _png_chunk(tag, payload):
    c = tag + payload
    return struct.pack(">I", len(payload)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)


def detect_output_mode(header, bgra, version):
    """决定 PNG 色彩模式：RGBA / RGB / 灰度 L，并完成 BGR→RGB 通道重排。

    TLG 内部 4 通道时必须带 alpha；3 通道固定 alpha=255 输出 RGB；
    TLG6 colors=1 输出 8 位灰度 L。bgra 为解码器的 B,G,R,A 字节序。
    """
    npix = len(bgra) // 4
    if header["colors"] == 1:
        return "L", bytes(bgra[i * 4] for i in range(npix))
    if header["colors"] == 4:
        rgb = bytearray(npix * 4)
        for i in range(npix):
            rgb[i * 4] = bgra[i * 4 + 2]      # R
            rgb[i * 4 + 1] = bgra[i * 4 + 1]  # G
            rgb[i * 4 + 2] = bgra[i * 4]      # B
            rgb[i * 4 + 3] = bgra[i * 4 + 3]  # A
        return "RGBA", bytes(rgb)
    # colors == 3：丢弃恒为 0xFF 的 alpha，缩小文件
    rgb = bytearray(npix * 3)
    for i in range(npix):
        rgb[i * 3] = bgra[i * 4 + 2]
        rgb[i * 3 + 1] = bgra[i * 4 + 1]
        rgb[i * 3 + 2] = bgra[i * 4]
    return "RGB", bytes(rgb)


def write_png_manual(path, width, height, mode, pixels):
    color_type = {"RGBA": 6, "RGB": 2, "L": 0}[mode]
    bpp = {"RGBA": 4, "RGB": 3, "L": 1}[mode]
    raw = bytearray()
    stride = width * bpp
    for y in range(height):
        raw.append(0)  # filter type 0 (None)
        raw += pixels[y * stride:(y + 1) * stride]
    idat = zlib.compress(bytes(raw), 9)
    ihdr = struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(_png_chunk(b"IHDR", ihdr))
        f.write(_png_chunk(b"IDAT", idat))
        f.write(_png_chunk(b"IEND", b""))


def write_png_pillow(path, width, height, mode, pixels):
    from PIL import Image
    img = Image.frombytes(mode, (width, height), pixels)
    img.save(path, "PNG")


def decode(data):
    """data -> (width, height, mode, pixels_bytes, info dict)。"""
    header = parse_header(data)
    if header["version"] == 5:
        bgra = decode_tlg5(data, header)
    else:
        bgra = decode_tlg6(data, header)
    mode, pixels = detect_output_mode(header, bgra, header["version"])
    info = dict(header)
    info["mode"] = mode
    info["pixel_bytes"] = len(bgra)
    return header["width"], header["height"], mode, pixels, info


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main(argv=None):
    ap = argparse.ArgumentParser(
        prog="tlg2png.py",
        description="KiriKiri .tlg (TLG5/TLG6) 图像解码为 PNG",
    )
    ap.add_argument("input", help="输入 .tlg 文件路径")
    ap.add_argument("output", nargs="?", default=None, help="输出 PNG 路径（缺省自动换扩展名）")
    ap.add_argument("--info", action="store_true", help="只打印头信息与解码统计")
    ap.add_argument("--via-pillow", action="store_true", help="用 Pillow 写 PNG（缺省手写 zlib PNG）")
    args = ap.parse_args(argv)

    try:
        with open(args.input, "rb") as f:
            data = f.read()
        width, height, mode, pixels, info = decode(data)
        if args.info:
            print("文件        : %s" % args.input)
            print("版本        : TLG%d" % info["version"])
            print("尺寸        : %d x %d" % (width, height))
            print("颜色分量    : %d (%s)" % (info["colors"],
                  {1: "8bit 灰度", 3: "24bit RGB", 4: "32bit RGBA"}[info["colors"]]))
            print("输出模式    : %s" % mode)
            print("解码像素    : %d 字节" % info["pixel_bytes"])
            print("数据偏移    : %d%s" % (info["data_offset"], " (旧式前缀)" if info["legacy"] else ""))
            return 0
        out_path = args.output
        if not out_path:
            import os
            base = os.path.splitext(args.input)[0]
            out_path = base + ".png"
        if args.via_pillow:
            try:
                write_png_pillow(out_path, width, height, mode, pixels)
            except ImportError:
                print("Pillow 不可用，回退到手写 PNG 编码", file=sys.stderr)
                write_png_manual(out_path, width, height, mode, pixels)
        else:
            write_png_manual(out_path, width, height, mode, pixels)
        print("已写入 %s（%s %dx%d）" % (out_path, mode, width, height))
        return 0
    except TLGError as e:
        print("错误: %s" % e, file=sys.stderr)
        return 2
    except OSError as e:
        print("IO 错误: %s" % e, file=sys.stderr)
        return 1
    except Exception as e:  # 防御：任何意外都给出可读信息
        print("意外错误: %r" % e, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
