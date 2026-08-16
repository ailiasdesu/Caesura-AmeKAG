#!/usr/bin/env python3
"""Regenerate src/render/EmbeddedShaders_GL.cpp / _Metal.cpp from shaderc output.

Prereqs:
  1. Build bgfx shaderc (bgfx GENie vs2022, build shaderc project).
  2. Run shaders/compile_shaders.bat linux + macos with SHADERC env set and
     an extra "-i <bgfx>/src" include arg (bgfx_shader.sh lives there).
  3. Run this script from the repo root.
"""
import os

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LINUX = os.path.join(BASE, "shaders", "compiled", "linux")
MACOS = os.path.join(BASE, "shaders", "compiled", "macos")
OUT = os.path.join(BASE, "src", "render")

FILES = ["vs_sprite", "vs_fullscreen", "stretch_blt_vs", "affine_blt_vs",
         "fs_texture", "fs_blend", "fs_transition", "fs_vfx",
         "fs_postfx_vignette", "fs_postfx_lut", "fs_postfx_blur", "fs_postfx_bloom",
         "stretch_blt_fs", "affine_blt_fs"]


def emit_c(name, data, out):
    out.append(f"const uint8_t {name}[] = {{")
    for i in range(0, len(data), 12):
        out.append("    " + ", ".join(f"0x{b:02X}" for b in data[i:i+12]) + ",")
    out.append("};")
    out.append(f"const size_t {name}_size = {len(data)};")
    out.append("")


def make(prefix, src_dir, ext, header_note):
    out = [f"// Auto-generated {header_note} embedded shaders.",
           "// Compiled from shaders/glsl/*.sc via bgfx shaderc.",
           '#include "EmbeddedShaders.h"', "", "namespace Caesura {", ""]
    for name in FILES:
        with open(os.path.join(src_dir, name + ext), "rb") as f:
            emit_c(f"kEmbedded{prefix}_{name}", f.read(), out)
    out.append("} // namespace Caesura")
    return "\n".join(out)


with open(os.path.join(OUT, "EmbeddedShaders_GL.cpp"), "w", encoding="utf-8", newline="\n") as f:
    f.write(make("GL", LINUX, ".bin", "OpenGL (GLSL)"))
with open(os.path.join(OUT, "EmbeddedShaders_Metal.cpp"), "w", encoding="utf-8", newline="\n") as f:
    f.write(make("Metal", MACOS, ".metal.bin", "Metal (MSL)"))
print("Regenerated EmbeddedShaders_GL.cpp + EmbeddedShaders_Metal.cpp")