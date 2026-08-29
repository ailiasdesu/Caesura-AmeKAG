#!/usr/bin/env python3
"""Repack a stale-layout bgfx shader (VSH11/FSH11) embedded array into the
current record layout, preserving the code section byte-for-byte.

Background (t71/t74/t75/t79): the embedded GL/Metal arrays in
src/render/EmbeddedShaders_*.cpp are complete shaderc BGFX binaries whose
GLSL/MSL text is correct; one array (kEmbeddedGL_fs_vfx) had TWO stale header
fields -- (1) uniform records without the texInfo/texFormat pair added for
bgfx binary version >= 8/>=10, (2) hashIn=0 which fails bgfx's
vertex/fragment hash matching (bgfx_p.h:5140
"Vertex shader output doesn't match fragment shader input").

The transform is the documented one in
docs/solutions/build-errors/bgfx-shader-binary-repack.md (same as 33a0a206).

Usage:
  python scripts/repack_gl_embed.py --array kEmbeddedGL_fs_vfx [--dry-run]
  python scripts/repack_gl_embed.py --array kEmbeddedGL_fs_vfx --hashin 3c3e1e6f
  python scripts/repack_gl_embed.py --array NAME --hashin-from-vs kEmbeddedGL_vs_fullscreen

--dry-run only reports the planned bytes and validation, never writes.
"""

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
GL_CPP = REPO / "src" / "render" / "EmbeddedShaders_GL.cpp"
DEFAULT_FILE = GL_CPP


def extract_hex(file_text: str, name: str) -> bytes:
    m = re.search(r"const uint8_t " + re.escape(name) + r"\[\] = \{(.*?)\};", file_text, re.S)
    if not m:
        sys.exit("ERROR: array %s not found in %s" % (name, DEFAULT_FILE))
    return bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1)))


def walk_uniforms(data, with_te):
    """Return (count, records, code_offset, code_size). Record shape for
    version >= 10: nameSize, name, type, num, regIndex u16, regCount u16,
    texInfo u16 (ver>=8), texFormat u16 (ver>=10)."""
    off = 12
    count = int.from_bytes(data[off:off + 2], "little")
    off += 2
    records = []
    for _ in range(count):
        ns = data[off]
        off += 1
        name = data[off:off + ns].decode("utf-8")
        off += ns
        type_ = data[off]
        num = data[off + 1]
        off += 2
        reg_index = int.from_bytes(data[off:off + 2], "little")
        off += 2
        reg_count = int.from_bytes(data[off:off + 2], "little")
        off += 2
        tex_info = tex_format = 0
        if with_te:
            tex_info = int.from_bytes(data[off:off + 2], "little")
            off += 2
            tex_format = int.from_bytes(data[off:off + 2], "little")
            off += 2
        records.append((name, type_, num, reg_index, reg_count, tex_info, tex_format))
    code_size = int.from_bytes(data[off:off + 4], "little")
    off += 4
    return count, records, off, code_size


def is_direct_feed_binary(data) -> bool:
    """Engine-identical probe (BgfxShaderManager::isDirectFeedBinary)."""
    if len(data) < 18:
        return False
    if data[0] not in (ord("V"), ord("F")):
        return False
    if data[1] != ord("S") or data[2] != ord("H") or data[3] < 6:
        return False
    p = 12
    count = int.from_bytes(data[p:p + 2], "little")
    p += 2
    for _ in range(count):
        if p >= len(data):
            return False
        ns = data[p]
        p += 1 + ns
        p += 6
        if data[3] >= 8:
            p += 2
        if data[3] >= 10:
            p += 2
        if p > len(data):
            return False
    if p + 4 > len(data):
        return False
    cs = int.from_bytes(data[p:p + 4], "little")
    p += 4
    if cs == 0 or p + cs + 1 > len(data):
        return False
    if data[p] != ord("#"):
        return False
    return data[p + cs] == 0


def repack(data, hashin):
    count, records, code_off, code_size = walk_uniforms(data, with_te=False)
    out = bytearray()
    out += data[0:4]                     # magic
    out += (hashin.to_bytes(4, "little") if hashin is not None else data[4:8])
    out += data[8:12]                    # hashOut
    out += count.to_bytes(2, "little")
    for (name, type_, num, reg_index, reg_count, _, _) in records:
        nb = name.encode("utf-8")
        out += bytes([len(nb)]) + nb + bytes([type_, num])
        out += reg_index.to_bytes(2, "little") + reg_count.to_bytes(2, "little")
        out += (0).to_bytes(2, "little") + (0).to_bytes(2, "little")  # texInfo/texFormat
    out += code_size.to_bytes(4, "little")
    if data[code_off] != ord("#"):
        raise SystemExit("ERROR: code section does not start with '#' -- not a text-coded binary?")
    out += data[code_off:]               # code + NUL + trailing bytes, byte-identical
    return bytes(out)


def emit_c_array(name, data) -> str:
    lines = []
    for i in range(0, len(data), 12):
        lines.append("    " + ", ".join("0x%02X" % b for b in data[i:i + 12]) + ",")
    return ("const uint8_t %s[] = {\n" % name + "\n".join(lines) + "\n};\n"
            "const size_t %s_size = %d;" % (name, len(data)))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--array", default="kEmbeddedGL_fs_vfx")
    ap.add_argument("--file", default=str(DEFAULT_FILE))
    ap.add_argument("--hashin", default=None, help="hex u32 for bytes 4..7")
    ap.add_argument("--hashin-from-vs", default=None, help="array name whose hashOut becomes hashIn")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    path = Path(args.file)
    text = path.read_text(encoding="utf-8")
    data = extract_hex(text, args.array)

    hashin = None
    if args.hashin is not None:
        hashin = int(args.hashin, 16)
    if args.hashin_from_vs:
        vs = extract_hex(text, args.hashin_from_vs)
        hashin = int.from_bytes(vs[8:12], "little")

    print("[repack] %s: len=%d current_layout_valid=%s hashIn=%s"
          % (args.array, len(data), is_direct_feed_binary(data), data[4:8].hex()))

    if is_direct_feed_binary(data) and hashin is None:
        print("[repack] array already in the current layout; nothing to do "
              "(use --hashin to fix an all-zero hashIn).")
        return 0

    new_data = repack(data, hashin)
    print("[repack] new len=%d valid=%s hashIn=%s"
          % (len(new_data), is_direct_feed_binary(new_data), new_data[4:8].hex()))
    if not is_direct_feed_binary(new_data):
        print("[repack] ERROR: repacked bytes fail validation", file=sys.stderr)
        return 1

    if args.dry_run:
        print("[repack] dry-run: no file written")
        return 0

    pat = re.compile(r"const uint8_t " + re.escape(args.array) +
                     r"\[\] = \{.*?\};\s*const size_t " + re.escape(args.array) +
                     r"_size = \d+;", re.S)
    if not pat.search(text):
        print("[repack] ERROR: array+size pattern not found", file=sys.stderr)
        return 1
    new_text = pat.sub(emit_c_array(args.array, new_data), text, count=1)
    path.write_text(new_text, encoding="utf-8", newline="\n")
    print("[repack] written %s" % path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
