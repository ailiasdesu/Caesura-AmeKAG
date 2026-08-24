# Project: Caesura (AmeKAG) Platform & Runtime Sprint

## Architecture
Caesura (AmeKAG) is a cross-platform Visual Novel engine with a strict 16-module modular architecture.
Module boundaries are enforced via `src/<module>/api/I<ModuleName>.h`. Direct implementation inclusion between modules is forbidden.
All backends are registered and accessed via `BackendRegistry`. `src/main.cpp` and `src/entry/` form the sole composition root.

### Data Flow & Component Interaction
- **Platform & Input**: `IPlatformBackend` encapsulates SDL3 window, IME text input, and event polling. `InputRouter` filters and routes events to KAG coroutines or game callbacks.
- **Scripting & Engine**: `DevCoreBinding` bridges platform capabilities to Lua sandbox. KAG Neo-Genesis interpreter manages coroutines and dynamic UI scenes.
- **Rendering & Animation**: `IRenderDevice` (bgfx) renders 2D scenes, Live2D Cubism models, and SMA 3D minigame meshes with cross-backend fallback (Direct3D 11, OpenGL, GLES, Metal).
- **Packaging & CI**: Automated pipelines for Android Release Signing / AAB generation and iOS Xcode / Metal toolchain hardening.

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | IPlatformBackend Text Input | Pure virtual methods `startTextInput()`, `stopTextInput()`, `setTextInputRect()`, `isTextInputActive()` | R1 | Survey 1 [DONE] |
| 2 | SDL3PlatformBackend Text Input | Implementation using `SDL_StartTextInput`, `SDL_StopTextInput`, `SDL_SetTextInputArea`, `SDL_TextInputActive` | R1 | Survey 1 [DONE] |
| 3 | NullPlatformBackend Text Input | Headless stubs for CI/testing without display | R1 | Survey 1 [DONE] |
| 4 | Engine Event Routing for IME | Dispatch `SDL_EVENT_TEXT_INPUT` (`_KAG_onTextInput`) and `SDL_EVENT_TEXT_EDITING` (`_KAG_onTextEditing`) | R1 | Survey 1 [DONE] |
| 5 | InputRouter IME Protection | Treat text input events as non-advancing under `InputFocus::KAG` | R1 | Survey 1 [DONE] |
| 6 | DevCore Lua IME Bindings | Expose `start_text_input`, `stop_text_input`, `set_text_input_rect`, `is_text_input_active` to Lua | R1 | Survey 1 [DONE] |
| 7 | Backend & Sandbox Whitelist | Bridge functions in `scripts/backend.lua` and allowlist in `scripts/sandbox.lua` | R1 | Survey 1 [DONE] |
| 8 | KAG [input] Schema Contract | Define `[input]` in `scripts/kag/schema.lua` with typed constraints | R1 | Survey 1 [DONE] |
| 9 | KAG [input] Interactive UI & Viewport Offset | Coroutine-yielding text box UI placed in upper viewport (`y <= 0.45 * height`) to avoid virtual keyboard occlusion | R1 | Survey 1 [DONE] |
| 10 | IME C++ & Lua Unit Tests | C++ doctests in `test_platform.cpp`, `test_input.cpp` and headless Lua tests in `test_input_cmd.lua` | R1 | Survey 1 [DONE] |
| 11 | Android Environment Signing Config | Dual env-var support (`CAESURA_ANDROID_*` & `CAESURA_*_PATH`) in `android/app/build.gradle` without hardcoded secrets | R2 | Survey 2 [DONE] |
| 12 | Android AAB & V1/V2/V3 Signing | Explicit V1/V2/V3 signing and `bundle { language { enableSplit false } }` configuration | R2 | Survey 2 [DONE] |
| 13 | Android Release Automation Scripts | PKCS12 keytool generator script (`generate_android_keystore.sh`) and release build script (`build_android_release.sh`) | R2 | Survey 2 [DONE] |
| 14 | Android CI Ephemeral Key Validation | End-to-end assembleRelease, bundleRelease, zipalign, and apksigner verification in CI | R2 | Survey 2 [DONE] |
| 15 | iOS CMake Toolchain Hardening | CMake iOS Xcode bundle definitions, framework linkages, and OpenSSL static slices | R3 | Survey 2 |
| 16 | Metal Shader Census & Verification | Python verification script (`verify_metal_shaders.py`) and C++ contract test (`test_render_metal_contract.cpp`) | R3 | Survey 2 |
| 17 | Metal Fallback Assertions | Document and assert Metal Post-FX identity fallback and SMA CPU skinning fallback | R3 | Survey 2 |
| 18 | iOS CI Workflow Hardening | SDL3 & OpenSSL dependency caching and robust build steps in `ci.yml` | R3 | Survey 2 |
| 19 | Live2D Multi-Backend Mobile Lifecycle | Validate Live2D Cubism memory bounds, native render paths, and Null fallback on mobile | R4 | Survey 3 |
| 20 | 3D Minigame & SMA GLES/Mobile Validation | Validate 3D primitives, physics loop, and dual-mode skinning (GPU compute / CPU SIMD fallback) | R4 | Survey 3 |
| 21 | Post-FX Stall-Free Degradation | Validate Vignette, LUT, SoftBlur, Bloom scratch RTT ping-pong without GPU pipeline stalls | R4 | Survey 3 |
| 22 | Full Regression Test Baseline | 100% C++ doctests (1041+ passing), 100% Lua tests (134+ passing), zero coupling violations across 16 modules | R4 | Survey 3 |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| R1 | IME Virtual Keyboard & Text Input | Features 1-10: IPlatformBackend, SDL3, InputRouter, Lua bindings, KAG [input] UI with viewport offset, C++ & Lua tests | none | DONE |
| R2 | Android Release Signing & AAB Pipeline | Features 11-14: build.gradle signing, PKCS12 keytool scripts, assembleRelease, bundleRelease, apksigner verify | none | DONE |
| R3 | iOS & Metal Toolchain / CI Hardening | Features 15-18: CMake iOS Xcode bundle, Metal shader verification, CI caching | none | IN_PROGRESS |
| R4 | Mobile Stress Validation & Baseline QA | Features 19-22: Live2D/3D minigame stress, Post-FX stall validation, full regression test suite pass | R1, R2, R3 | PLANNED |

## Interface Contracts
### `src/platform/api/IPlatformBackend.h`
```cpp
virtual void startTextInput() = 0;
virtual void stopTextInput() = 0;
virtual void setTextInputRect(int x, int y, int w, int h, int cursor = 0) = 0;
virtual bool isTextInputActive() const = 0;
```

### `src/script/bindings/DevCoreBinding.cpp` ↔ Lua
```lua
DevCore.start_text_input() -> void
DevCore.stop_text_input() -> void
DevCore.set_text_input_rect(x, y, w, h, cursor) -> void
DevCore.is_text_input_active() -> boolean
```

### KAG Neo-Genesis `[input]` Command
```kag
[input name="player_name" prompt="Enter your name:" default="Hero" maxlen=16 x=360 y=200 width=1200 height=120]
```
- Coerces to `f.player_name` by default.
- Yields current coroutine until confirmed or canceled.
- Positions UI at `y <= 0.45 * height` to prevent virtual keyboard occlusion.

## Code Layout
- `src/platform/api/IPlatformBackend.h`: Platform backend interface
- `src/platform/SDL3PlatformBackend.h`, `SDL3PlatformBackend.cpp`: SDL3 implementation
- `src/platform/NullPlatformBackend.h`: Headless mock implementation
- `src/entry/Engine.cpp`: Main loop & event dispatch to `_KAG_onTextInput` / `_KAG_onTextEditing`
- `src/input/InputRouter.cpp`: Event filtering for KAG non-advancing inputs
- `src/script/bindings/DevCoreBinding.cpp`: Lua DevCore bindings
- `scripts/backend.lua`: Engine backend wrapper
- `scripts/sandbox.lua`: Lua sandbox whitelist
- `scripts/kag/schema.lua`: Schema definitions for `[input]`
- `scripts/kag/commands/text.lua`: KAG text & interactive input implementation
- `android/app/build.gradle`: Android release signing and bundle configuration
- `scripts/generate_android_keystore.sh`, `scripts/build_android_release.sh`: Android scripts
- `CMakeLists.txt`, `.github/workflows/ci.yml`: CMake & CI build hardening
- `scripts/verify_metal_shaders.py`: Metal shader validation tool
- `tests/cpp/`: C++ doctest suites (`test_platform.cpp`, `test_input.cpp`, `test_render_metal_contract.cpp`)
- `tests/scripts/`: Lua test suites (`test_input_cmd.lua`, `run_lua_tests.lua`)
