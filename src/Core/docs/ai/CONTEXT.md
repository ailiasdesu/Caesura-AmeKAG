# Caesura (AmeKAG) — AI Development Context

## What this project is

Cross-platform visual novel engine (C++20, SDL3 + bgfx + SoLoud + Lua 5.4).
Target user: artists/writers with zero programming experience.
AI-assisted KAG script development via IDE tools (Codex, Copilot, Cursor).

## How to generate KAG scripts

1. Read docs/api/KAG-API.md for the complete function reference
2. Read scripts/sandbox.lua to understand safety boundaries
3. Use docs/templates/scene-template.lua as a starting point
4. All KAG functions live under the global KAG table
5. Convenience methods (show_text, show_image, clear_screen, wait_click) have their canonical implementation in _CAESURA_BACKEND
6. File paths for assets: relative paths resolved through the IAssetProvider chain (directory -> XP3 -> CARC)

## Architecture summary

`
main.cpp -> Engine -> BackendRegistry (singleton, pure query)
                  -> IRenderDevice (bgfx)
                  -> IAudioBackend (SoLoud)
                  -> IPlatformBackend (SDL3)
Lua scripts -> KAG table -> C++ bindings -> BackendRegistry
`

## Key files

- STRATEGY.md — product strategy and tracks
- ENGINE_ANALYSIS.md — full codebase technical analysis
- docs/api/KAG-API.md — KAG API reference
- docs/templates/scene-template.lua — scene template
- scripts/sandbox.lua — sandbox rules (AI-readable)
- docs/plans/ — implementation plans
- src/ — engine source code (9 modules)
