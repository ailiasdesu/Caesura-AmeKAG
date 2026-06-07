# Caesura (AmeKAG) - AI Development Context

## What this project is

Cross-platform visual novel engine (C++20, SDL3 + bgfx + SoLoud + Lua 5.4).
Target user: artists/writers with zero programming experience.
AI-assisted KAG script development via IDE tools (Codex, Copilot, Cursor).

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
- Particle effects (emitters with full config, per-frame update)
- Change resolution and toggle fullscreen at runtime
- Switch input focus between KAG (visual novel) and GAME (mini-game) modes
- Debug diagnostics and JSON error reports
- Async texture loading with callbacks
- Batch quad rendering for performance

### What the engine CANNOT do (yet)
- Video playback (stub functions return safe defaults; FFmpeg planned for Beta)
- 3D rendering (bgfx capable, no script-level API exposed yet)
- Animation/tweening system (planned for Beta)
- Choice menus (must be scripted manually in Lua with click detection)
- Auto-save/skip/backlog (planned)
- Localization/i18n (planned)
- Network/multiplayer (out of scope)

## Architecture Summary

```
main.cpp -> Engine -> BackendRegistry (singleton, pure query)
                  -> IRenderDevice (bgfx)
                  -> IAudioBackend (SoLoud)
                  -> IPlatformBackend (SDL3)
Lua scripts -> KAG table -> C++ bindings -> BackendRegistry
```

## Module Map

| Module | Global | Functions | Purpose |
|---|---|---|---|
| KAG | KAG | 35 | Core VN: audio, images, text, save/load |
| Render | Render | 28 | Low-level: textures, viewports, batch/blend |
| VFX | VFX | 10 | Particle system |
| Unified | _CAESURA_BACKEND | 7 | Delegation proxy to Render/KAG/DevCore |
| DevCore | DevCore | 8 | Engine control: resolution, fullscreen, quit |
| Debug | Debug | 10 | Diagnostics: errors, stats, JSON reports |
| Save | (on KAG) | 6 | Save/load game state |

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

## Key Files

- `STRATEGY.md` - product strategy and 4-track plan
- `ENGINE_ANALYSIS.md` - full codebase technical analysis
- `docs/api/KAG-API-REFERENCE.md` - **complete API reference (98+ functions)**
- `docs/templates/scene-template.lua` - scene template
- `scripts/sandbox.lua` - sandbox rules (AI-readable)
