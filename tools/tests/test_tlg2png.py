#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_tlg2png.py - tools/tlg2png.py 的合成样本自洽测试
=======================================================

没有真实 .tlg 样本可用（格式为 KiriKiri 专有），因此本测试采用
"已知像素模式 + 最小实现" 自洽验证法：

1. 用测试内自带的**合成编码器**（enc_* 系列）从已知像素模式构造合法
   TLG5 / TLG6 二进制流——编码器是解码器算法的严格镜像（同一套
   Golomb 表 / LZSS 语义 / 差分方程），方向不同而已；
2. 用 tlg2png.py 的解码路径解码回像素，断言与原始像素**逐字节相等**；
3. 解码产物走手写 PNG 编码器写出 .png，再用独立的最小 PNG 读取器
   （zlib + 反滤波）读回，断言像素一致——验证 PNG 输出独立正确；
4. Pillow 可用时再交叉打开一次，双保险。

该法验证了：头解析、LZSS 解压（含 match 路径、扩展长度、跨平面/跨块
窗口延续）、TLG5 差分重组、TLG6 Golomb 位流（零段/非零段/转义长码）+
滤波预测 + zigzag 扫描、通道组装与 PNG 封装。

运行：python tools/tests/test_tlg2png.py
"""
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import tlg2png as T   # noqa: E402

FAILURES = []


def check(name, cond, detail=""):
    if cond:
        print("  ok   %s" % name)
    else:
        print("  FAIL %s  %s" % (name, detail))
        FAILURES.append((name, detail))


# ---------------------------------------------------------------------------
# 最小 LZSS 编码器（tlg2png.decompress_slide 的严格镜像，含 match 路径）
# ---------------------------------------------------------------------------

class LzssCtx:
    """携带与解码器一致的 4096 字节环形窗口状态（跨平面/块延续）。"""

    def __init__(self, preset=None):
        self.window = bytearray(preset) if preset is not None else bytearray(4096)
        self.r = 0

    def _find_match(self, data, i):
        """窗口内找最长 match。必须与解码器的重叠拷贝语义逐位一致。

        对每个候选源位置 pos，在**真实窗口**上逐字节重放解码器拷贝，
        并对照源串累加 match 长；一旦不同即停。
        """
        best_len = 0
        best_pos = 0
        n = len(data)
        local = self.window
        wmask = 4095
        maxlen = min(n - i, 272)
        for pos in range(4096):
            if local[pos] != data[i]:
                continue
            tmp = bytearray(local)   # 模拟窗口拷贝，不改真实窗口
            mm = pos
            rr = self.r
            l = 0
            while l < maxlen and tmp[mm] == data[i + l]:
                b = tmp[mm]
                tmp[rr] = b
                mm = (mm + 1) & wmask
                rr = (rr + 1) & wmask
                l += 1
            if l > best_len:
                best_len = l
                best_pos = pos
                if l >= maxlen:
                    break
        return (best_pos, best_len) if best_len >= 3 else None

    def _write_match(self, mpos, mlen):
        local = self.window
        wmask = 4095
        mm = mpos
        rr = self.r
        for _ in range(mlen):
            b = local[mm]
            local[rr] = b
            mm = (mm + 1) & wmask
            rr = (rr + 1) & wmask
        self.r = rr


def enc_lzss(data, ctx):
    """修正 LZSS 编码（8 token/控制字节，LSB 先行；匹配 12bit + 4bit + 扩展）。"""
    out = bytearray()
    tokens = []
    ctrl = 0
    ctrl_bits = 0
    i = 0
    n = len(data)

    def emit_group():
        nonlocal ctrl, ctrl_bits
        out.append(ctrl & 0xFF)
        for tok in tokens:
            out.extend(tok)
        tokens.clear()
        ctrl = 0
        ctrl_bits = 0

    while i < n:
        best = ctx._find_match(data, i)
        if best is not None:
            mpos, mlen = best
            # 不越过本次编码剩余量（自重试匹配可能比剩余更长）
            if mlen > n - i:
                mlen = n - i
            if mlen < 3:
                mlen = 0
        if best is not None and mlen >= 3:
            l4 = min(mlen, 18) - 3
            tok = bytearray([mpos & 0xFF, (l4 << 4) | ((mpos >> 8) & 0x0F)])
            cnt = l4 + 3
            if mlen > 18:     # l4 == 15 → 解码器必读扩展字节
                tok.append(min(mlen - 18, 255))
                cnt = 18 + tok[-1]
            ctx._write_match(mpos, cnt)
            i += cnt
            ctrl |= 1 << ctrl_bits
            tokens.append(tok)
        else:
            b = data[i]
            ctx.window[ctx.r] = b
            ctx.r = (ctx.r + 1) & 4095
            i += 1
            ctrl |= 0 << ctrl_bits
            tokens.append(bytes([b]))
        ctrl_bits += 1
        if ctrl_bits == 8:
            emit_group()
    if ctrl_bits:
        emit_group()
    return bytes(out)


# ---------------------------------------------------------------------------
# TLG5 合成编码器
# ---------------------------------------------------------------------------

def enc_tlg5(rgba, width, height, colors=3, blockheight=None, force_raw=False):
    """从 RGBA 像素构造 TLG5 二进制流（严格镜像 tlg2png 的 compose 方程）。

    返回 (payloads 列表 [block][plane] → mark+size+data, blockheight)。
    差分方程（与 _compose_first_line/_compose_line 互逆）：
      首行：o1=g 的 dG；o0=(dB)-o1；o2=(dR)-o1（dB/dR 相对行内前像素）
      后续：dX = X - 上邻 X；o1 = dG-dGprev；o0 = (dB-dBprev)-o1...
    """
    if colors == 3:
        if any(rgba[i * 4 + 3] != 0xFF for i in range(width * height)):
            raise ValueError("colors=3 模式 alpha 必须全为 0xFF")
    if blockheight is None:
        blockheight = height

    ctx = LzssCtx()
    payloads = []
    for y_blk in range(0, height, blockheight):
        y_lim = min(y_blk + blockheight, height)
        rows = y_lim - y_blk
        plane = [bytearray(rows * width) for _ in range(colors)]
        bufpos = 0
        for y in range(y_blk, y_lim):
            db_prev = dg_prev = dr_prev = da_prev = 0
            for x in range(width):
                idx = (y * width + x) * 4
                r = rgba[idx]; g = rgba[idx + 1]; b = rgba[idx + 2]; a = rgba[idx + 3]
                if y == 0:
                    db, dg, dr, da = b, g, r, a
                else:
                    up = ((y - 1) * width + x) * 4
                    db = (b - rgba[up + 2]) & 0xFF   # up-B
                    dg = (g - rgba[up + 1]) & 0xFF   # up-G
                    dr = (r - rgba[up]) & 0xFF       # up-R
                    da = (a - rgba[up + 3]) & 0xFF
                o1 = (dg - dg_prev) & 0xFF
                o0 = ((db - db_prev) - o1) & 0xFF
                o2 = ((dr - dr_prev) - o1) & 0xFF
                plane[0][bufpos + x] = o0
                plane[1][bufpos + x] = o1
                plane[2][bufpos + x] = o2
                if colors == 4:
                    plane[3][bufpos + x] = (da - da_prev) & 0xFF
                db_prev, dg_prev, dr_prev, da_prev = db, dg, dr, da
            bufpos += width
        for c in range(colors):
            if force_raw:
                payloads.append(bytes([1]) + struct.pack("<I", rows * width)
                                + bytes(plane[c]))
            else:
                payload = enc_lzss(bytes(plane[c]), ctx)
                payloads.append(bytes([0]) + struct.pack("<I", len(payload)) + payload)
    return payloads, blockheight


def assemble_tlg5(width, height, colors, payloads, blockheight):
    blockcount = (height - 1) // blockheight + 1
    per_block = len(payloads) // blockcount
    sizes = []
    for bi in range(blockcount):
        sizes.append(sum(len(payloads[bi * per_block + c]) for c in range(per_block)))
    head = T.TLG5_SIG + bytes([colors]) + struct.pack("<III", width, height, blockheight)
    table = b"".join(struct.pack("<I", s) for s in sizes)
    return head + table + b"".join(payloads)


# ---------------------------------------------------------------------------
# TLG6 Golomb 编码器（tlg2png._decode_golomb_channel 的严格镜像）
# ---------------------------------------------------------------------------

class BitWriter:
    def __init__(self):
        self.bytes = bytearray()
        self.bitpos = 0

    def write_bit(self, b):
        if self.bitpos >> 3 >= len(self.bytes):
            self.bytes.append(0)
        self.bytes[self.bitpos >> 3] |= (b & 1) << (self.bitpos & 7)
        self.bitpos += 1

    def write_bits_lsb_first(self, value, n):
        for j in range(n):
            self.write_bit((value >> j) & 1)

    def write_unary(self, zeros):
        """zeros 个 0 后跟一个 1。"""
        for _ in range(zeros):
            self.write_bit(0)
        self.write_bit(1)


def golomb_encode(values):
    """有符号差分列表 → (pool bytes, 总位数)。严格镜像解码器：

    bit0 = 首段是否全零；随后交替段：unary 计数(zeros+1) + zeros 位二进制
    后缀；非零段每值：自适应 k=表[a][n]，unary(z=v>>k) + k 位符号-幅度后缀。
    转义：若 unary 的 1 放不进当前 32 位窗口（z+pos>=32），改写满窗零 +
    1 字节零计数 + k 位后缀（对应解码器 t==0 路径）。
    """
    from tlg2png import _GB_TABLE
    bw = BitWriter()
    n = len(values)
    if n == 0:
        return bytes(), 0
    runs = []
    cur_zero = (values[0] == 0)
    start = 0
    for i in range(1, n):
        z = values[i] == 0
        if z != cur_zero:
            runs.append((cur_zero, values[start:i]))
            cur_zero = z
            start = i
    runs.append((cur_zero, values[start:]))

    bw.write_bit(0 if runs[0][0] else 1)
    a = 0
    ncnt = T.T6_GOLOMB_N_COUNT - 1

    def write_count(count):
        zeros = count.bit_length() - 1
        bw.write_unary(zeros)
        bw.write_bits_lsb_first(count - (1 << zeros), zeros)

    def write_value(v_full, k):
        # 严格镜像 krkrz CompressValuesGolomb：
        #   m = ((e>=0)?2e:-2e-1)-1；unary 写 m>>k 个 0 再跟 1；
        #   用 GetBytePos(=bitpos//8) 跟踪，超过 GOLOMB_GIVE_UP_BYTES(4) 字节就
        #   "give up"：把 m>>k 写成 8 位字面字节（LSB-first），不再写终止 1。
        m = v_full
        zeros = m >> k
        store_limit = bw.bitpos // 8 + 4
        put1 = True
        for _ in range(zeros):
            if store_limit == bw.bitpos // 8:
                bw.write_bits_lsb_first(m >> k, 8)
                put1 = False
                break
            bw.write_bit(0)
        if store_limit == bw.bitpos // 8:
            bw.write_bits_lsb_first(m >> k, 8)
            put1 = False
        if put1:
            bw.write_bit(1)
        bw.write_bits_lsb_first(m, k)

    for is_zero, chunk in runs:
        write_count(len(chunk))
        if not is_zero:
            for val in chunk:
                if val == 0:
                    raise ValueError("值 0 必须走零段")
                k = _GB_TABLE[a][ncnt]
                mag = abs(val)
                v_full = ((mag - 1) << 1) | (1 if val > 0 else 0)
                write_value(v_full, k)
                a += mag - 1
                if a >= 1024:
                    a = 1023
                ncnt -= 1
                if ncnt < 0:
                    a >>= 1
                    ncnt = T.T6_GOLOMB_N_COUNT - 1
    return bytes(bw.bytes), bw.bitpos


def enc_lzss_preset(filter_types):
    ctx = LzssCtx(preset=T.build_tlg6_preset_window())
    return enc_lzss(bytes(filter_types), ctx)


def extract_deltas(image, width, height, colors, filter_types):
    """解码器状态机镜像：对目标像素反解各通道差分，存入解码同序槽位。

    返回逐 8 行组、逐通道的差分数组（与 tlg2png 行组装完全同构）。
    """
    xbc = (width - 1) // 8 + 1
    main_count = width // 8
    fraction = width - main_count * 8
    zerocolor = 0xFF000000 if colors == 3 else 0
    zeroline = [zerocolor] * width
    prevline = zeroline
    prevline_index = 0
    groups = []

    def sim_line(chan_out, prevline, pi_init, ci_init, width,
                 start_block, block_limit, ftypes, ft_idx, skipbytes,
                 idx_base, oddskip, dir_):
        pi = pi_init
        ci = ci_init
        if start_block:
            pi += start_block * 8
            ci += start_block * 8
            p = image[ci - 1]
            up = prevline[pi - 1]
        else:
            p = up = zerocolor
        idx = idx_base + skipbytes * start_block
        step = 1 if (dir_ & 1) else -1
        for i in range(start_block, block_limit):
            w = width - i * 8
            if w > 8:
                w = 8
            ww = w
            if step == -1:
                idx += ww - 1
            if i & 1:
                idx += oddskip * ww
            decoder = T.FILTERS[ftypes[ft_idx + i]]
            while w:
                u = prevline[pi]
                pred = decoder(p, u, up, 0)
                tgt = image[ci]
                idxvp = idx
                for c in range(colors):
                    v = ((tgt >> (c * 8)) & 0xFF) - ((pred >> (c * 8)) & 0xFF)
                    if v > 127:
                        v -= 256
                    elif v < -128:
                        v += 256
                    chan_out[c][idxvp] = v
                ddelta = 0
                for c in range(colors):
                    ddelta |= (chan_out[c][idxvp] & 0xFF) << (c * 8)
                p = decoder(p, u, up, ddelta & 0xFFFFFFFF)
                up = u
                ci += 1
                pi += 1
                idx += step
                w -= 1
            if step == 1:
                idx += skipbytes - ww
            else:
                idx += skipbytes + 1
            if i & 1:
                idx -= oddskip * ww

    for y in range(0, height, 8):
        ylim = min(y + 8, height)
        pixel_count = (ylim - y) * width
        chans = [[0] * pixel_count for _ in range(colors)]
        ft = (y // 8) * xbc
        skipbytes = (ylim - y) * 8
        for yy in range(y, ylim):
            curline = yy * width
            dir_ = (yy & 1) ^ 1
            oddskip = (ylim - yy - 1) - (yy - y)
            if main_count:
                start = (width if width < 8 else 8) * (yy - y)
                sim_line(chans, prevline, prevline_index, curline, width,
                         0, main_count, filter_types, ft, skipbytes,
                         start, oddskip, dir_)
            if main_count != xbc:
                ww = min(fraction, 8)
                start = ww * (yy - y)
                sim_line(chans, prevline, prevline_index, curline, width,
                         main_count, xbc, filter_types, ft, skipbytes,
                         start, oddskip, dir_)
            prevline = image
            prevline_index = curline
        groups.append(chans)
    return groups


def enc_tlg6(rgba, width, height, colors=4, filter_kind="mix"):
    """从 RGBA 像素构造 TLG6 二进制流。colors=1 取 R 通道为灰度（与测试断言一致）。"""
    image = []
    for i in range(width * height):
        r = rgba[i * 4]; g = rgba[i * 4 + 1]; b = rgba[i * 4 + 2]; a = rgba[i * 4 + 3]
        if colors == 1:
            image.append(r)
        elif colors == 3:
            image.append(0xFF000000 | (r << 16) | (g << 8) | b)
        else:
            image.append((a << 24) | (r << 16) | (g << 8) | b)
    xbc = (width - 1) // 8 + 1
    ybc = (height - 1) // 8 + 1
    ft = bytearray(xbc * ybc)
    for by in range(ybc):
        for bx in range(xbc):
            if filter_kind == "all0":
                ft[by * xbc + bx] = 0
            elif filter_kind == "all1":
                ft[by * xbc + bx] = 1
            else:
                ft[by * xbc + bx] = (bx + by) % 2

    groups = extract_deltas(image, width, height, colors, ft)
    blob = bytearray()
    total_bits = 0
    for chans in groups:
        for c in range(colors):
            pool, nbits = golomb_encode(chans[c])
            total_bits += nbits
            blob += struct.pack("<I", nbits)   # 方法 00 (Golomb)
            blob += pool
    ft_lzss = enc_lzss_preset(ft)
    head = (T.TLG6_SIG + bytes([colors, 0, 0, 0])
            + struct.pack("<III", width, height, total_bits)
            + struct.pack("<I", len(ft_lzss)) + ft_lzss)
    return head + bytes(blob)


# ---------------------------------------------------------------------------
# 独立 PNG 读取器（校验手写 PNG 编码器）
# ---------------------------------------------------------------------------

def read_png(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "PNG 签名错误"
    off = 8
    idat = bytearray()
    w = h = ct = None
    while off < len(data):
        (ln,) = struct.unpack_from(">I", data, off)
        tag = data[off + 4:off + 8]
        payload = data[off + 8:off + 8 + ln]
        if tag == b"IHDR":
            w, h, bitd, ct = struct.unpack(">IIBB", payload[:10])
            assert bitd == 8
        elif tag == b"IDAT":
            idat += payload
        off += 12 + ln
    bpp = {0: 1, 2: 3, 6: 4}[ct]
    raw = zlib.decompress(bytes(idat))
    stride = w * bpp
    out = bytearray()
    prev = bytearray(stride)
    for y in range(h):
        ft = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        if ft == 1:
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif ft == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ft == 3:
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif ft == 4:
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        out += line
        prev = line
    return w, h, ct, bytes(out)


# ---------------------------------------------------------------------------
# 测试用例
# ---------------------------------------------------------------------------

def make_rgba(width, height, fn):
    buf = bytearray()
    for y in range(height):
        for x in range(width):
            r, g, b, a = fn(x, y)
            buf += bytes((r & 0xFF, g & 0xFF, b & 0xFF, a & 0xFF))
    return bytes(buf)


def assert_bgra_matches(bgra, rgba, w, h, has_alpha, label):
    ok = all(
        bgra[i * 4] == rgba[i * 4 + 2]        # B
        and bgra[i * 4 + 1] == rgba[i * 4 + 1]
        and bgra[i * 4 + 2] == rgba[i * 4]    # R
        and (bgra[i * 4 + 3] == (rgba[i * 4 + 3] if has_alpha else 0xFF))
        for i in range(w * h))
    check(label, ok)


def _png_roundtrip(tmp, name, w, h, bgra, rgba, has_alpha):
    """解码 BGRA → detect_output_mode 重排 → 手写 PNG → 独立读取器回读。"""
    hdr = {"colors": 4 if has_alpha else 3}
    mode, pixels = T.detect_output_mode(hdr, bgra, 0)
    check("%s detect 模式" % name, mode == ("RGBA" if has_alpha else "RGB"))
    png = os.path.join(tmp, name + ".png")
    T.write_png_manual(png, w, h, mode, pixels)
    pw, ph, pct, ppix = read_png(png)
    check("%s PNG 头" % name, pw == w and ph == h and pct == (6 if has_alpha else 2))
    if has_alpha:
        ok = all(ppix[i * 4] == rgba[i * 4] and ppix[i * 4 + 1] == rgba[i * 4 + 1]
                 and ppix[i * 4 + 2] == rgba[i * 4 + 2] and ppix[i * 4 + 3] == rgba[i * 4 + 3]
                 for i in range(w * h))
    else:
        ok = all(ppix[i * 3] == rgba[i * 4] and ppix[i * 3 + 1] == rgba[i * 4 + 1]
                 and ppix[i * 3 + 2] == rgba[i * 4 + 2] for i in range(w * h))
    check("%s PNG 像素回读" % name, ok)
    # Pillow 交叉验证（可选）
    try:
        from PIL import Image
        img = Image.open(png)
        pv = list(img.convert("RGBA").tobytes())
        ok2 = all(pv[i * 4] == rgba[i * 4] and pv[i * 4 + 1] == rgba[i * 4 + 1]
                  and pv[i * 4 + 2] == rgba[i * 4 + 2]
                  and pv[i * 4 + 3] == (rgba[i * 4 + 3] if has_alpha else 255)
                  for i in range(w * h))
        check("%s Pillow 交叉验证" % name, ok2)
    except ImportError:
        pass


def test_lzss_matches():
    print("LZSS 单元测试（match / 扩展长度 / 窗口延续）")
    import random
    data = bytes(random.Random(7).randrange(0, 256) for _ in range(80)) * 6
    ctx = LzssCtx()
    payload = enc_lzss(data, ctx)
    ctx2 = LzssCtx()
    out = bytearray(len(data))
    T.decompress_slide(out, payload, ctx2.window, 0)
    check("LZSS 重复数据往返", bytes(out) == data, "payload=%d" % len(payload))

    z = bytes(300)   # 长零串 → 长度扩展 (>18) 路径
    ctx = LzssCtx()
    payload = enc_lzss(z, ctx)
    ctx2 = LzssCtx()
    out = bytearray(len(z))
    T.decompress_slide(out, payload, ctx2.window, 0)
    check("LZSS 长零串扩展长度", bytes(out) == z)

    S1 = b"hello world hello world hello world"
    S2 = b"world hello world hello world hello"
    ctx = LzssCtx()
    p1 = enc_lzss(S1, ctx)
    p2 = enc_lzss(S2, ctx)
    ctx2 = LzssCtx()
    o1 = bytearray(len(S1)); o2 = bytearray(len(S2))
    r = T.decompress_slide(o1, p1, ctx2.window, 0)
    T.decompress_slide(o2, p2, ctx2.window, r)
    check("LZSS 窗口跨调用延续", bytes(o1) == S1 and bytes(o2) == S2)


def test_tlg5_rgb(tmp):
    print("TLG5 RGB (colors=3, 单块, LZSS)")

    def fn(x, y):
        return (x * 7 + y * 3, x * 3 + y * 5, x * 11 + y * 13, 255)
    w, h = 33, 17
    rgba = make_rgba(w, h, fn)
    payloads, bh = enc_tlg5(rgba, w, h, colors=3)
    data = assemble_tlg5(w, h, 3, payloads, bh)
    hdr = T.parse_header(data)
    check("TLG5 头解析", hdr["version"] == 5 and hdr["colors"] == 3
          and hdr["width"] == w and hdr["height"] == h)
    out = T.decode_tlg5(data, hdr)
    assert_bgra_matches(out, rgba, w, h, False, "TLG5 RGB 像素一致")
    _png_roundtrip(tmp, "t5_rgb", w, h, out, rgba, False)
    return data


def test_tlg5_rgba_blocks(tmp):
    print("TLG5 RGBA (colors=4, blockheight=16 多块, 跨块窗口延续)")

    def fn(x, y):
        return ((x * 5) ^ (y * 3), (x * 2 + y) & 0xFF, (x + y * 9) & 0xFF,
                (x * 13 + y * 7) & 0xFF)
    w, h = 45, 37
    rgba = make_rgba(w, h, fn)
    payloads, bh = enc_tlg5(rgba, w, h, colors=4, blockheight=16)
    assert bh == 16
    data = assemble_tlg5(w, h, 4, payloads, bh)
    hdr = T.parse_header(data)
    out = T.decode_tlg5(data, hdr)
    assert_bgra_matches(out, rgba, w, h, True, "TLG5 RGBA 多块像素一致")
    _png_roundtrip(tmp, "t5_rgba", w, h, out, rgba, True)
    return data


def test_tlg5_raw_path(tmp):
    print("TLG5 RAW 平面路径 (mark!=0)")

    def fn(x, y):
        return ((x + y) & 0xFF, (x * 2) & 0xFF, (y * 3) & 0xFF, 255)
    w, h = 20, 11
    rgba = make_rgba(w, h, fn)
    payloads, bh = enc_tlg5(rgba, w, h, colors=3, force_raw=True)
    data = assemble_tlg5(w, h, 3, payloads, bh)
    hdr = T.parse_header(data)
    out = T.decode_tlg5(data, hdr)
    assert_bgra_matches(out, rgba, w, h, False, "TLG5 RAW 路径像素一致")
    return data


def test_tlg6_rgba(tmp):
    print("TLG6 RGBA (colors=4, 滤波 0/1 混合, 非 8 倍数尺寸, 大差分/转义)")

    def fn(x, y):
        if y < 4 and x < 6:
            return (0, 0, 0, 0)          # 全零区 → 零段
        if y >= h - 2:
            return (200, 100, 50, 255)   # 平坦区（与前一行产生大差分）
        return (x * 3 & 0xFF, y * 5 & 0xFF, (x + y) * 2 & 0xFF,
                (x * 7 + y * 3) & 0xFF)
    global h
    h = 24
    w = 40
    rgba = make_rgba(w, h, fn)
    data = enc_tlg6(rgba, w, h, colors=4, filter_kind="mix")
    hdr = T.parse_header(data)
    check("TLG6 头解析", hdr["version"] == 6 and hdr["colors"] == 4
          and hdr["width"] == w and hdr["height"] == h)
    out = T.decode_tlg6(data, hdr)
    assert_bgra_matches(out, rgba, w, h, True, "TLG6 RGBA 像素一致")
    _png_roundtrip(tmp, "t6_rgba", w, h, out, rgba, True)
    return data


def test_tlg6_rgb(tmp):
    print("TLG6 RGB (colors=3)")

    def fn(x, y):
        return (255 - x, (x * y) & 0xFF, 128 + (y & 1) * 40, 255)
    w, h = 39, 18
    rgba = make_rgba(w, h, fn)
    data = enc_tlg6(rgba, w, h, colors=3, filter_kind="all0")
    hdr = T.parse_header(data)
    out = T.decode_tlg6(data, hdr)
    assert_bgra_matches(out, rgba, w, h, False, "TLG6 RGB 像素一致")
    _png_roundtrip(tmp, "t6_rgb", w, h, out, rgba, False)
    return data


def test_tlg6_gray(tmp):
    print("TLG6 灰度 (colors=1)")

    def fn(x, y):
        return (x ^ y, 0, 0, 255)
    w, h = 24, 17
    rgba = make_rgba(w, h, fn)
    data = enc_tlg6(rgba, w, h, colors=1, filter_kind="mix")
    hdr = T.parse_header(data)
    out = T.decode_tlg6(data, hdr)
    g = [out[i * 4] for i in range(w * h)]
    check("TLG6 灰度像素一致", all(g[i] == rgba[i * 4] for i in range(w * h)))
    mode, pixels = T.detect_output_mode(hdr, out, 6)
    check("灰度输出模式 L", mode == "L" and len(pixels) == w * h)
    png = os.path.join(tmp, "t6_gray.png")
    T.write_png_manual(png, w, h, "L", pixels)
    pw, ph, pct, ppix = read_png(png)
    check("灰度 PNG 回读", pw == w and ph == h and pct == 0
          and all(ppix[i] == g[i] for i in range(len(g))))
    return data


def test_narrow_width(tmp):
    print("窄图 (width < 8, TLG6 fraction 路径)")

    def fn(x, y):
        return (x * 30 + y, 120, 60, 200)
    w, h = 5, 12
    rgba = make_rgba(w, h, fn)
    data = enc_tlg6(rgba, w, h, colors=4, filter_kind="mix")
    hdr = T.parse_header(data)
    out = T.decode_tlg6(data, hdr)
    assert_bgra_matches(out, rgba, w, h, True, "窄图 TLG6 像素一致")
    return data


def test_tiny1x1():
    print("1x1 极简图")
    w, h = 1, 1
    rgba = bytes((12, 34, 56, 200))
    data = enc_tlg6(rgba, w, h, colors=4, filter_kind="all0")
    hdr = T.parse_header(data)
    out = T.decode_tlg6(data, hdr)
    assert_bgra_matches(out, rgba, w, h, True, "1x1 TLG6 像素一致")
    rgba5 = bytes((12, 34, 56, 255))
    payloads, bh = enc_tlg5(rgba5, w, h, colors=3)
    data5 = assemble_tlg5(w, h, 3, payloads, bh)
    hdr5 = T.parse_header(data5)
    out5 = T.decode_tlg5(data5, hdr5)
    assert_bgra_matches(out5, rgba5, w, h, False, "1x1 TLG5 像素一致")


def test_unsupported_method():
    print("TLG6 不支持熵编码方法 → 报错")

    def fn(x, y):
        return (x, y, 0, 255)
    w, h = 16, 8
    rgba = make_rgba(w, h, fn)
    data = bytearray(enc_tlg6(rgba, w, h, colors=4, filter_kind="all0"))
    # 定位第一个通道头：sig 11 + colors/flags 4 = 15；w/h/max_bits 12 → 27；
    # 滤波流 at 27：u32 size + payload
    off = 27
    inbuf = struct.unpack_from("<I", data, off)[0]
    off += 4 + inbuf
    w0 = struct.unpack_from("<I", data, off)[0]
    struct.pack_into("<I", data, off, (1 << 30) | (w0 & 0x3FFFFFFF))  # method=01 Gamma
    try:
        T.decode_tlg6(bytes(data), T.parse_header(bytes(data)))
        check("method=01(Gamma) 报错", False)
    except T.TLGError:
        check("method=01(Gamma) 报错", True)


def test_errors():
    print("错误路径")
    try:
        T.parse_header(b"TLGX.0\x00raw\x1a" + bytes(40))
        check("无效签名被拒绝", False)
    except T.TLGError:
        check("无效签名被拒绝", True)

    try:
        T.parse_header(T.TLG6_SIG + bytes([2]) + bytes(20))
        check("TLG6 非法 colors 被拒绝", False)
    except T.TLGError:
        check("TLG6 非法 colors 被拒绝", True)

    try:
        T.parse_header(T.TLG6_SIG + bytes([4, 1, 0, 0]) + struct.pack("<III", 8, 8, 8))
        check("非零 dataflag 被拒绝", False)
    except T.TLGError:
        check("非零 dataflag 被拒绝", True)

    try:
        T.parse_header(b"XXXYYY" + bytes(40))
        check("混淆变体被拒绝", False)
    except T.TLGError:
        check("混淆变体被拒绝", True)

    # 截断：头部完整但数据流被砍掉 → 解码应报错
    def fn2(x, y):
        return (x, y, 0, 255)
    w, h = 16, 8
    rgba = make_rgba(w, h, fn2)
    data = enc_tlg6(rgba, w, h, colors=4, filter_kind="all0")
    raised = False
    try:
        # 只保留头部（截止到 max_bit_length 之后、滤波流被切断）
        cut = data[: 27]  # 签名11 + colors/flags 4 + w/h/max_bits 12
        T.decode_tlg6(cut, T.parse_header(cut))
    except T.TLGError:
        raised = True
    check("截断 TLG6 报错", raised)


def main():
    import tempfile
    tmp = tempfile.mkdtemp(prefix="tlg_test_")
    test_lzss_matches()
    test_tlg5_rgb(tmp)
    test_tlg5_rgba_blocks(tmp)
    test_tlg5_raw_path(tmp)
    test_tlg6_rgba(tmp)
    test_tlg6_rgb(tmp)
    test_tlg6_gray(tmp)
    test_narrow_width(tmp)
    test_tiny1x1()
    test_unsupported_method()
    test_errors()
    print()
    if FAILURES:
        print("失败 %d 项:" % len(FAILURES))
        for name, detail in FAILURES:
            print("  - %s %s" % (name, detail))
        return 1
    print("全部通过 OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
