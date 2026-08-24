# Milestone R4 & Baseline Zero-Regression QA Survey Report

**Date**: 2026-08-24  
**Investigator**: Explorer Agent (`explorer_survey_3`)  
**Target Milestone**: R4 (Live2D, 3D Minigame & Post-FX Mobile Stress Validation) & Engine-Wide Zero-Regression QA  

---

## Executive Summary

This investigation performed a comprehensive architectural and runtime survey across all 16 modules of the Caesura (AmeKAG) visual novel engine. All 1028 C++ doctest test cases (315,931 assertions) and all 133 Lua test suite scripts passed with **zero failures and zero regressions** (`100% pass, 0 failed, 0 skipped`). The architectural coupling audit (`python scripts/count_coupling.py`) confirmed that all 16 modules strictly adhere to the AGENTS.md charter coupling limits.

The Live2D, 3D Minigame, and Post-Processing rendering pipelines feature robust multi-backend abstractions (D3D11, OpenGL/GLES, Metal, SPIR-V/Vulkan) with graceful degradation, zero-stall execution paths, and CPU fallbacks suitable for mobile and resource-constrained environments.

---

## 1. Live2D & 3D Minigame Architecture and Lifecycle

### 1.1 Live2D Backend (`src/live2d/`)

The Live2D subsystem implements the `IAnimationBackend` interface (`src/live2d/api/IAnimationBackend.h`) with two distinct implementations:
- **`Live2DBackend`** (`src/live2d/Live2D/Live2DBackend.h`, `Live2DBackend.cpp`): Full Cubism 5 SDK integration.
- **`NullAnimationBackend`** (`src/live2d/NullAnimationBackend.h`, `NullAnimationBackend.cpp`): SDK-less fallback that maps model requests to static sprite textures (PNG/JPG/BMP), ensuring headless CI and SDK-free platforms run deterministically without crashes.

#### Render Path Abstraction (`ILive2DRenderPath`)
To bridge Cubism's native rendering output with the engine's `bgfx` compositing pipeline, `ILive2DRenderPath` provides backend-specific bridges:
1. **`D3D11NativeRenderPath`** (Windows): Renders to an offscreen D3D11 render target view (RTV) and shares the Shader Resource View (SRV) directly with `bgfx`.
2. **`OpenGLSharedRenderPath`** (Linux / Android GLES): Renders to an offscreen GL FBO (`GL_RGBA8`), uses GPU-side `glBlitFramebuffer`, and injects the native texture into `bgfx` via `bgfx::overrideInternal` for zero-copy compositing.
3. **`OpenGLReadbackRenderPath`** (Fallback / GLES compatibility): Dedicated FBO with synchronous `glReadPixels`, vertical origin flip, and `bgfx::updateTexture2D`. Used as an automatic fallback when shared FBOs are unsupported.
4. **`MetalNativeRenderPath`** (macOS / iOS): Utilizes shared `MTLDevice` from `bgfx::getInternalData()->context`, renders to `MTLTexture` offscreen, and synchronizes back to `bgfx` texture handles.

#### Security & Confinement (`PathConfinement.cpp`)
All Live2D asset loading (`.moc3`, `.model3.json`, motions, expressions, textures) is strictly gated by `confineToModelRoot()`. Path traversal attempts (e.g. `../`, absolute paths) are blocked, and input buffer allocations are strictly bounded (<256 MB per model file) to prevent memory exhaustion attacks.

#### Lifecycle Flow
```
Engine::init()
  └─► Live2DBackend::init()
        ├─► CubismFramework::StartUp(&allocator, &option)
        ├─► CubismFramework::Initialize()
        └─► ILive2DRenderPath::init(1280, 720)
Script/KAG Loop
  ├─► loadModel(path, name) -> returns model handle
  ├─► showModel(handle, x, y, scale)
  ├─► playMotion(handle, motion_name) / setExpression() / setParameter()
  ├─► render(dt) -> m_renderPath->beginFrame() -> DrawModel() -> m_renderPath->endFrame() -> blitTexture()
  └─► unloadModel(handle)
Engine::shutdown()
  └─► Live2DBackend::shutdown()
        ├─► Clear models & destroy bgfx textures
        ├─► m_renderPath->shutdown()
        └─► CubismFramework::Dispose()
```

---

### 1.2 3D Minigame Backend (`src/minigame/`)

The 3D mini-game subsystem provides embedded 3D scene execution (`IMiniGameBackend.h`, `BgfxMiniGameBackend.cpp`):

#### Key Components:
- **Geometry Cache (`MiniGeometry.cpp`)**: Pre-allocates and caches vertex/index buffers for standard 3D primitives: Cube, Subdivided Sphere (16 segments), Plane, and Quad.
- **Lighting Model (`MiniLight.h`)**: Supports ambient light, directional sunlight with color/intensity, and up to 3 dynamic point lights (`MAX_POINT_LIGHTS = 3`) with attenuation radius.
- **Physics & Collision (`MiniCollision.cpp`)**: AABB and Sphere collision detection with velocity, gravity acceleration, and dispatch of collision callbacks to Lua (`on_collision`).
- **Hot-Path Zero-Allocation**: `runCollisionDetection()` utilizes pre-reserved member vectors (`m_colIds`, `m_colX`, `m_colY`, `m_colZ`, `m_colSx`, `m_colSy`, `m_colSz`) initialized at startup, avoiding any runtime heap allocation per frame.
- **Shader Pipeline (`EmbeddedMiniGameShaders.h`)**:
  - Direct3D 11/12: Precompiled DXBC bytecode (`kEmbeddedDXBC_MiniGame_VS/FS`).
  - OpenGL / GLES: Embedded GLSL source (`kEmbeddedGLSL_MiniGame_VS/FS`).
  - Apple Metal: Embedded MSL source (`kEmbeddedMSL_MiniGame_VS/FS`).
  - Fallback: Graceful validation check with clean fallback.

#### Lifecycle & Mode Switching:
- `loadScene(path)`: Parses JSON scene definition into CPU entities.
- `enter(sceneHandle)`: Hides 2D visual novel layers, switches input focus to `GAME`, and activates the 3D camera loop.
- `update(dt)`: Main-thread physics simulation, gravity integration, and collision callbacks.
- `render()`: Submits 3D objects with view/projection matrices to `MINIGAME_VIEW = 10`.
- `leave()`: Restores 2D VN layers and normal dialogue flow.
- `shutdown()`: Destroys GPU buffers, shader programs, and uniform handles.

---

### 1.3 Skeletal Mesh Animation (SMA) & Skinning (`src/render/SmaMeshRenderer.cpp`)

The engine provides dual-mode Skeletal Mesh Animation:
1. **S2 CPU Soft-Skinning (`SmaSkinner.h`)**: Pure CPU SIMD-friendly transform skinning feeding transient vertex buffers. Guaranteed 100% portable on all mobile targets (Android GLES / iOS Metal / Web).
2. **S5 GPU Compute Skinning (`SmaMeshRenderer.cpp`)**: GPU compute shader skinning using static vertex buffers and structured bone transform buffers. `SkinMode::Auto` inspects `bgfx::getCaps()->supported & BGFX_CAPS_COMPUTE`. On platforms lacking compute shaders (e.g. standard GLES 3.0 / low-end mobile), it automatically and silently falls back to S2 CPU skinning without dropped frames or pipeline faults.

---

## 2. Render Post-Processing Pipeline & Mobile Optimization

### 2.1 Post-FX Pipeline Architecture (`src/render/BgfxRenderDevice.cpp`)

The post-processing subsystem provides four standard full-screen effects configured via `[vfx postfx=...]` in KAG scripts:
1. **`Vignette`**: Radial darkening with controllable radius, softness, and color tint.
2. **`LutColorGrade`**: 3D LUT / 2D strip color grading with controllable blend mix.
3. **`SoftBlur`**: Fast single-pass / multi-tap separable gaussian blur.
4. **`Bloom`**: Multi-pass HDR bright extract + downsample (1/2 res) $\rightarrow$ blur pass 1 (1/4 res) $\rightarrow$ blur pass 2 (1/4 res) $\rightarrow$ additive composite onto backbuffer.

### 2.2 Stall-Free Pipeline & RTT Ping-Pong
- **Ping-Pong Buffering**: Multi-stage post-processing chains alternate between scratch render targets (`PostFxRt` slots 0 and 1) so that no shader stage reads from the texture it is currently writing to, avoiding framebuffer feedback hazards and GPU pipeline flushes.
- **Downsampled Bloom**: Bloom isolates downsampling to $1/2$ and $1/4$ resolution scratch buffers (`slot 4, 5, 6`), reducing fillrate overhead by over 75% on mobile GPUs.
- **No Synchronous CPU-GPU Readbacks**: Standard rendering submit paths (`submitFullscreenQuad`, `submitBlend`, `submitTransition`, `submitVFX`) execute strictly asynchronously via `bgfx::submit()`. No `glFinish()`, `glReadPixels()`, or GPU fence waits are invoked during normal gameplay frames.

### 2.3 Shader Compilation & Cross-Platform Fallbacks (`BgfxShaderManager.cpp`)
- **ESSL 3.00 Dynamic Translation (`toEssl300`)**: On OpenGLES / Android targets, legacy desktop GLSL bytecode headers and syntax (`attribute`, `varying`, `gl_FragColor`, `#version 120/430`) are automatically translated in memory to valid ESSL 3.00 (`in`, `out`, `oFragColor`, `#version 300 es`), preventing shader compile failures on Adreno / Mali mobile drivers.
- **Fallback Chains**: If dedicated post-processing pixel shaders are unavailable on a particular backend (e.g. Vulkan or bare GLES), the pipeline degrades cleanly to `m_fallbackProgram` / `fsTexture` (identity pass), ensuring game logic continues without graphics device crashes.

---

## 3. Test Suite Verification & Quality Metrics

### 3.1 C++ Unit & Integration Test Suite (`tests/cpp/`)
Execution of `build/tests/Debug/CaesuraTests.exe`:
- **Test Cases**: 1028 / 1028 passed (100%)
- **Assertions**: 315,931 / 315,931 passed (100%)
- **Failed**: 0
- **Skipped**: 0
- **Execution Time**: ~4.2 seconds

Key test coverage domains verified:
- `test_live2d.cpp`: Path confinement, null backend fallback, model loading, motion playback, expression/parameter caching.
- `test_minigame.cpp` & `test_mini_game.cpp`: IMiniGameBackend lifecycle, JSON scene parsing, object spawning, physics, collision detection.
- `test_render_postfx.cpp` & `test_render_pipeline.cpp`: PostFxKind enum contracts, parameter limits, NullRenderDevice degradation, RTT pool lifecycle.
- `test_mobile_adapter.cpp`: Touch event mapping (pinch-to-wheel, long-press right click, multi-touch tracking), display scaling, low memory & pause/resume lifecycle.
- `test_sma_skinner.cpp` & `test_mesh_renderer.cpp`: S2 CPU skinning math, dual-bone weighting, GPU compute fallback.

### 3.2 Lua Script & KAG Engine Test Suite (`tests/scripts/`)
Execution of `run_lua_tests.lua`:
- **Total Test Files**: 133 / 133 passed (100%)
- **Failed**: 0
- **Passed Areas**: Tokenizer, KAG commands, Scheduler, Compiler, Bytecode cache, CARC import, Schema validation & coercion, Flow edge cases, Rollback memory, I18n, Post-FX bindings, Scale stress.

### 3.3 Benchmark & Stress Baseline
- **Scale Stress (`test_scale_stress.lua`)**:
  - 4096$\times$4096 texture atlas: 4096 tiles / 16.77M texels tracked in <1s.
  - Audio handle pool: >100 concurrent voice/SE handles with ID recycling across 20,000 churn iterations.
  - 10,000-token scene: parsed in ~40ms, scheduler walked 9600+ frames without drift.
  - 500-page backlog: capped at desktop limits with heap growth strictly bounded (<4MB).
  - 3,000-line narrative flow: expression translation sustained at ~3.5 $\mu$s/line.
- **CPU Performance (`test_perf_bench.cpp`)**:
  - Lua 10k format+append: ~18ms (budget <800ms).
  - Lua 10k table reads: ~1.2ms (budget <400ms).
  - SmaSkinner 8k-vertex dual-bone soft-skinning: ~1.1ms/frame (budget <10ms).

---

## 4. Module Coupling Architecture Audit

Running `python scripts/count_coupling.py` confirms that the entire codebase adheres strictly to the module dependency constraints defined in AGENTS.md:

| Module | Max Allowed | Current Count | Status | Dependent Modules |
| :--- | :---: | :---: | :---: | :--- |
| `archive` | $\le 4$ | **2** | PASS | `debug:1, resource:1` |
| `audio` | $\le 4$ | **2** | PASS | `debug:1, di:2` |
| `debug` | $\le 4$ | **0** | PASS | (None) |
| `di` | $\le 14$ | **13** | PASS | `archive:1, audio:2, debug:1, input:1, job:1, live2d:1, minigame:1, platform:4, render:6, resource:2, script:1, steam:1, storage:1` |
| `entry` | $\le 14$ | **14** | PASS | (Composition root - all 14 target modules) |
| `input` | $\le 4$ | **0** | PASS | (None) |
| `job` | $\le 4$ | **1** | PASS | `di:1` |
| `live2d` | $\le 4$ | **3** | PASS | `debug:6, di:1, render:3` |
| `minigame` | $\le 4$ | **4** | PASS | `debug:1, di:1, input:1, render:1` |
| `platform` | $\le 4$ | **0** | PASS | (None) |
| `render` | $\le 4$ | **4** | PASS | `audio:1, debug:14, di:16, job:1` |
| `resource` | $\le 4$ | **3** | PASS | `debug:3, di:1, job:1` |
| `rpc` | $\le 4$ | **2** | PASS | `archive:2, debug:1` |
| `script` | $\le 14$ | **11** | PASS | `audio:2, debug:5, di:12, input:1, job:1, minigame:2, platform:3, render:9, resource:2, steam:1, storage:1` |
| `steam` | $\le 4$ | **0** | PASS | (None) |
| `storage` | $\le 4$ | **4** | PASS | `archive:1, debug:1, di:1, steam:1` |

---

## 5. Mobile Stress Validation & Zero-Regression QA Plan (R4)

To guarantee zero regressions and optimal battery/thermal performance across iOS (Metal) and Android (GLES/Vulkan), the following stress validation plan is established:

```
+-------------------------------------------------------------------------------+
|                       R4 Mobile Stress Validation Matrix                      |
+-------------------------------------------------------------------------------+
| Test Track          | Target Conditions          | Verification Metric        |
+---------------------+----------------------------+----------------------------+
| 1. Live2D Mobile    | 3 simultaneous Live2D      | < 8ms render time          |
|    Load & Render    | models + motion playback   | Zero GL/Metal leak         |
|                     | on GLES/Metal              | Clean fallback to Null     |
+---------------------+----------------------------+----------------------------+
| 2. 3D Minigame      | 50 dynamic objects +       | Stable 60 FPS              |
|    Stress           | 3 point lights + collision | 0 heap allocs in update()  |
+---------------------+----------------------------+----------------------------+
| 3. Post-FX Chain    | Bloom + Vignette + LUT     | Downsampled bloom (1/4 res)|
|    Fillrate Guard   | running at 1080p/4K        | Ping-pong RTs, no stall    |
+---------------------+----------------------------+----------------------------+
| 4. Lifecycle &      | Background pause (onPause) | Instant state recovery     |
|    Memory Pressure  | Low-memory warning         | Texture cache pruned       |
|                     | Foreground resume (onResume| 0 dangling GPU handles     |
+---------------------+----------------------------+----------------------------+
| 5. Touch & IME      | Rapid multi-touch, pinch,  | Clean gesture scaling      |
|    Input Stress     | long-press, text input     | Viewport offset on keyboard|
+---------------------+----------------------------+----------------------------+
```

### 5.1 Concrete Verification Steps:
1. **Automated Headless Regression**:
   - `build\tests\Debug\CaesuraTests.exe` $\rightarrow$ 1028/1028 passed.
   - `external\lua\lua.exe tests\scripts\run_lua_tests.lua` $\rightarrow$ 133/133 passed.
   - `python scripts\count_coupling.py` $\rightarrow$ 0 coupling limit violations.
2. **Mobile Lifecycle Verification**:
   - Verify `MobileAdapter::onPause` / `onResume` preserves Lua execution context and texture budget.
   - Verify `onLowMemory` triggers `TextureManager` budget eviction to Tier 0 (128 MB) without crashing active scene state.
3. **Shader Pipeline Validation**:
   - Verify all embedded shaders compile cleanly across D3D11, GLES (via ESSL 3.00 rewrite), and Metal.
   - Verify S5 compute skinning degrades to S2 CPU soft-skinning whenever `BGFX_CAPS_COMPUTE` is absent.

---

## Conclusion
The Caesura engine's Live2D, 3D Minigame, and Post-FX subsystems are architecturally sound, thoroughly tested, and equipped with robust mobile fallbacks. The engine maintains a 100% green test baseline across all 16 modules.
