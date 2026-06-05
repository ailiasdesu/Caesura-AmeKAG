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
    stretch_blt_fs.metal
    affine_blt_fs.metal

  dx11/                        ← [SOURCE + ARTIFACT] HLSL sources + compiled DXBC
    *.hlsl                     ← [SOURCE] HLSL input to fxc.exe
    *.dxbc                     ← [ARTIFACT] compiled DXBC binary (intermediate)
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
                          [dx11/*.dxbc] ──manual──→ [src/Render/EmbeddedShaders.cpp]
```
- For D3D11/D3D12 backends, DXBC byte arrays are embedded directly in `src/Render/EmbeddedShaders.cpp`.
- For Vulkan, SPIR-V byte arrays are embedded in `src/Render/EmbeddedShaders.cpp` + `EmbeddedShaders_SPIRV.cpp`.
- Metal shaders can be compiled via `shaderc --platform osx --profile metal` and then embedded.

## How to add a new shader
1. Create `glsl/<name>.sc` (bgfx shaderc dialect)
2. Create `dx11/<name>.hlsl` (HLSL source)
3. Compile HLSL: `fxc /T vs_4_0|ps_4_0 /E main /Fo dx11/<name>.dxbc dx11/<name>.hlsl`
4. Convert `.dxbc` → C array, append to `src/Render/EmbeddedShaders.cpp`
5. Add `extern` declaration to `src/Render/EmbeddedShaders.h`
6. Wire program creation in `BgfxRenderDevice::initEmbeddedShaders()`
7. Create `metal/<name>.metal` for macOS reference
