# Caesura (AmeKAG) — AI Development Context

## What this is

Cross-platform visual novel engine. C++20, SDL3 + bgfx + SoLoud + Lua 5.4.
Target: artists/writers with zero programming, assisted by AI IDE tools.

## How to generate KAG scripts

1. Read `docs/api/KAG-API.md` — 35 KAG functions documented
2. Read `scripts/sandbox.lua` — safety boundaries
3. Start from `docs/templates/scene-template.lua`
4. All functions under global `KAG` table
5. `show_text`, `show_image`, `clear_screen`, `wait_click` — canonical path is `_CAESURA_BACKEND`
6. Asset paths resolve through IAssetProvider chain (directory → XP3 → CARC)

## Architecture

```
main.cpp → Engine → BackendRegistry (query singleton)
                 → IRenderDevice (bgfx)
                 → IAudioBackend (SoLoud)
                 → IPlatformBackend (SDL3)
Lua → KAG table → C++ bindings → BackendRegistry
```

## Key files

| File | Purpose |
|------|---------|
| `STRATEGY.md` | Product strategy |
| `ENGINE_ANALYSIS.md` | Technical analysis |
| `docs/api/KAG-API.md` | KAG API reference |
| `docs/templates/scene-template.lua` | Scene template |
| `scripts/sandbox.lua` | Sandbox rules |
| `docs/plans/` | Implementation plans |
| `src/` | Engine source (9 modules) |