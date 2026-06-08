# Caesura (AmeKAG) - AI Development Context

## What this project is

Cross-platform visual novel engine (C++20, SDL3 + bgfx + SoLoud + Lua 5.4).
Target user: artists/writers with zero programming experience.
AI-assisted KAG script development via IDE tools (Codex, Copilot, Cursor).

**Cross-Platform Status:** Windows (MSVC) / macOS (Clang) / Linux (GCC) — GitHub Actions CI 全绿。

## How to generate KAG scripts

1. Read `docs/api/KAG-API-REFERENCE.md` for the complete function reference (98+ functions, 7 modules)
2. Read `scripts/sandbox.lua` to understand safety boundaries and resource quotas
3. Use `docs/templates/scene-template.lua` as a starting point
4. All KAG functions live under the global `KAG` table
5. Convenience methods (show_text, show_image, clear_screen, wait_click) have their canonical implementation in `_CAESURA_BACKEND`
6. File paths for assets: relative paths resolved through the IAssetProvider chain (directory -> XP3 -> CARC)

## Engine Capabilities (Quick Glance)

### What the engine CAN do
- Display images (backgrounds, character sprites, UI overlays) on bg/fg/msg layers
- Play background music with cross-fade (WAV/MP3/OGG)
- Play voice lines (absolute interrupt, auto-advance on complete)
- Play 2D and 3D sound effects
- Control audio volume per bus (bgm/voice/se) with fade
- Display dialogue text in message box
- Wait for user click to advance
- Render text at arbitrary screen positions with color
- Render ruby/furigana text (Japanese reading aids)
- Screen shake effects (KAG.quake)
- Save/load game state (6 save slots, JSON state, metadata)
- Particle effects (emitters with full config, per-frame update via JobSystem)
- Change resolution and toggle fullscreen at runtime
- Switch input focus between KAG (visual novel) and GAME (mini-game) modes
- Debug diagnostics and JSON error reports
- Async texture loading with callbacks
- Batch quad rendering for performance
- Video playback (FFmpeg, non-blocking decode thread)
- CARC archive decryption (multi-threaded, BCrypt/OpenSSL)

### What the engine CANNOT do (yet)
- 3D rendering (bgfx capable, IGamePlugin interface reserved, no script-level API yet)
- Animation/tweening system (planned for Beta)
- Choice menus (must be scripted manually in Lua with click detection)
- Auto-save/skip/backlog (planned)
- Localization/i18n (planned)
- Network/multiplayer (out of scope)
- Live2D integration (planned for Beta)

## Architecture Summary

```
main.cpp -> Engine -> BackendRegistry (singleton, pure query)
                  -> IRenderDevice (bgfx)
                  -> IAudioBackend (SoLoud)
                  -> IPlatformBackend (SDL3)
                  -> JobSystem (thread pool + lock-free queue)
Lua scripts -> KAG table -> C++ bindings -> BackendRegistry
```

## Multi-Threading Architecture

**Principle:** Main storyline runs single-threaded sequentially. Multi-core is reserved for non-storyline paths.

| Subsystem | Thread Model |
|---|---|
| Main Loop (Engine) | Main thread, sequential |
| JobSystem | Thread pool, lock-free queue — for CPU tasks |
| CARC Decryption | Background thread pool (BCrypt Win / OpenSSL cross-platform) |
| Texture Loading | Background load + main thread callback |
| FFmpeg Video Decode | Independent decode thread, non-blocking main loop |
| Particle System | JobSystem per-frame update, parallelized |
| Audio (SoLoud) | Internal mixing thread (SoLoud-managed) |

## Module Map

| Module | Global | Functions | Purpose |
|---|---|---|---|
| KAG | KAG | 35 | Core VN: audio, images, text, save/load |
| Render | Render | 28 | Low-level: textures, viewports, batch/blend |
| VFX | VFX | 10 | Particle system (JobSystem parallelized) |
| Unified | _CAESURA_BACKEND | 7 | Delegation proxy to Render/KAG/DevCore |
| DevCore | DevCore | 8 | Engine control: resolution, fullscreen, quit |
| Debug | Debug | 10 | Diagnostics: errors, stats, JSON reports |
| Save | (on KAG) | 6 | Save/load game state |
| Video | (internal) | — | FFmpeg decode, non-blocking |

## Key Rules for AI-Generated Code

1. **All KAG scripts run in a sandbox** (see scripts/sandbox.lua)
2. **No filesystem access** - all asset loading through KAG API functions
3. **500k instruction budget per frame** - keep loops bounded
4. **Resource quotas**: 256 textures max, 64 audio handles, 8 RTTs, 16 particle emitters
5. **_G is write-protected** - can only assign to whitelisted globals
6. **Scene entry point**: define scene_start() and optionally scene_end()
7. **Dialogue pattern**: KAG.show_text() + KAG.wait_click() for each line
8. **For character sprites**: use layer "fg", position them relative to 1280x720 base resolution
9. **For backgrounds**: use layer "bg" at position 0,0
10. **Japanese text support**: use KAG.render_ruby() for furigana, KAG.render_text() for labels

## Cross-Platform Build Rules

When modifying C++ engine code, follow these rules to keep CI green:

1. **Windows API guards**: All `#include <windows.h>` and Win32 API calls must be wrapped in `#ifdef _WIN32`
2. **MSVC CRT guards**: `_CrtSetReportHook` etc. must use `#ifdef _MSC_VER` for both includes and function bodies
3. **C standard library**: Clang/GCC require explicit `#include <cstring>` for `memcpy`/`strcmp` — do not rely on implicit MSVC includes
4. **CMake definitions**: Platform-specific compile definitions use `$<CONFIG:Debug>` generator expressions, not `if(MSVC)` blocks
5. **macOS Metal**: bgfx Metal backend requires linking `Foundation.framework` in both main and test targets
6. **doctest arrays**: `extern const T arr[]` passed to `CHECK()` must be cast to `const void*` for macOS Clang compatibility

## Key Files

- `STRATEGY.md` - product strategy and 8-track plan
- `ENGINE_ANALYSIS.md` - full codebase technical analysis
- `docs/api/KAG-API-REFERENCE.md` - **complete API reference (98+ functions)**
- `docs/templates/scene-template.lua` - scene template
- `scripts/sandbox.lua` - sandbox rules (AI-readable)
- `docs/solutions/build-error/cross-platform-ci-fixes.md` - CI cross-platform fixes reference
