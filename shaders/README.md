# Caesura (AmeKAG) - Shader Pipeline

## Directory Layout
```
shaders/
  README.md                    ← this file
  compile_shaders.sh           ← [TOOL] Unix: compile .sc → platform binaries via bgfx shaderc
  compile_shaders.bat          ← [TOOL] Windows: same

  glsl/                        ← [SOURCE] bgfx shaderc (.sc) dialect — input to shaderc
    vs_sprite.sc               → OpenGL VS, D3D HLSL, Metal MSL
    vs_fullscreen.sc           → fullscreen quad VS (procedural, no vertex buffer)
    stretch_blt_vs.sc          → stretch-blit VS (quad via attribute buffer)
    affine_blt_vs.sc           → affine-blit VS (2x3 matrix row-major)
    fs_texture.sc              → simple 2D texture lookup FS
    fs_blend.sc                → 8-mode blend FS (add/sub/mul/scr/ovl/drk/lit/dif + lerp)
    fs_transition.sc           → crossfade/rule/wipe transition FS
    fs_vfx.sc                  → VFX FS (fade, 3x3 blur, quake)
    fs_postfx_vignette.sc      → round-102 postfx chain: radial vignette FS
    fs_postfx_lut.sc           → round-102 postfx chain: color-grade (LUT) FS
    fs_postfx_blur.sc          → round-102 postfx chain: soft gaussian blur FS
    fs_postfx_bloom.sc         → round-102 postfx chain: bloom composite FS
    stretch_blt_fs.sc          → stretch-blit FS (src_rect UV offset/scale)
    affine_blt_fs.sc           → affine-blit FS (passthrough tex lookup)

  metal/                       ← [SOURCE] Metal Shading Language — standalone reference
    vs_sprite.metal
    vs_fullscreen.metal
    stretch_blt_vs.metal
    affine_blt_vs.metal
    fs_texture.metal
    fs_blend.metal
    fs_transition.metal
    fs_vfx.metal
    fs_postfx_vignette.metal
    fs_postfx_lut.metal
    fs_postfx_blur.metal
    fs_postfx_bloom.metal
    stretch_blt_fs.metal
    affine_blt_fs.metal

  dx11/                        ← [SOURCE + ARTIFACT] HLSL sources + compiled DXBC
    *.hlsl                     ← [SOURCE] HLSL input to fxc.exe
    *.dxbc                     ← [ARTIFACT] compiled DXBC binary (intermediate)
    fs_postfx_vignette.hlsl/.dxbc  ← round-102 postfx chain FS (vignette)
    fs_postfx_lut.hlsl/.dxbc       ← round-102 postfx chain FS (color grade)
    fs_postfx_blur.hlsl/.dxbc      ← round-102 postfx chain FS (soft blur)
    fs_postfx_bloom.hlsl/.dxbc     ← round-102 postfx chain FS (bloom)
    embedded_new_shaders.cpp   ← [ARTIFACT] C arrays from DXBC (staging area)
    embedded_new_shaders.h     ← [ARTIFACT] extern declarations (staging area)

  compiled/                    ← [ARTIFACT] output of compile_shaders.* (not in repo)
    linux/                     ← OpenGL GLSL binary shaders
    macos/                     ← Metal binary shaders
    windows/                   ← DXBC binary shaders
```

## Compilation Flow
```
[glsl/*.sc] ──shaderc──→ [compiled/<platform>/*.bin]
                               │
                               ▼ (DXBC path)
                          [dx11/*.dxbc] ──manual──→ [src/render/EmbeddedShaders.cpp]
```
- For D3D11/D3D12 backends, DXBC byte arrays are embedded directly in `src/render/EmbeddedShaders.cpp`.
- For Vulkan, SPIR-V byte arrays are embedded in `src/render/EmbeddedShaders.cpp` + `EmbeddedShaders_SPIRV.cpp`.
- Metal shaders can be compiled via `shaderc --platform osx --profile metal` and then embedded.

## How to add a new shader
1. Create `glsl/<name>.sc` (bgfx shaderc dialect)
2. Create `dx11/<name>.hlsl` (HLSL source)
3. Compile HLSL: `fxc /T vs_4_0|ps_4_0 /E main /Fo dx11/<name>.dxbc dx11/<name>.hlsl`
4. Convert `.dxbc` → C array, append to `src/render/EmbeddedShaders.cpp`
5. Add `extern` declaration to `src/render/EmbeddedShaders.h`
6. Wire program creation in `BgfxRenderDevice::initEmbeddedShaders()`
7. Create `metal/<name>.metal` for macOS reference

## Round-102 post-processing chain (vignette / LUT grade / soft blur / bloom)

The four post-fx fragment shaders ride the same full-screen-quad pipeline and
share one `PostFxParams` uniform (4x vec4 = 64 bytes):

| Index | Components | Meaning |
|-------|-----------|---------|
| [0]   | strength, radius, amount, lutMix | per-kind master params |
| [1]   | r, g, b, 1 | tint / grade color |
| [2]   | 1/w, 1/h, 0, 0 | texel size (blur/bloom) |
| [3]   | 0,0,0,0 | spare |

- **Primary (D3D11/D3D12):** real compiled DXBC, baked into
  `src/render/EmbeddedShaders.cpp` (shared with `fs_blend`/`fs_vfx`).
- **GL / Metal / Vulkan:** the chain falls back to the identity texture copy
  (no visual effect) because shaderc bytecode for these backends is not
  embedded yet. To enable them: `./compile_shaders.sh all` (or the .bat) with
  `SHADERC` pointing at bgfx shaderc, then run `embed_to_c.py`, and select the
  per-backend bytecode in `BgfxShaderManager::initEmbeddedShaders()` (the
  `fsPostfx*` Bytecode locals already exist for that).

Bloom is internally multi-pass (bright-pass extract + ½-res downsample,
2x ¼-res blur, additive composite) but is exposed as a single `PostFxStage`.
