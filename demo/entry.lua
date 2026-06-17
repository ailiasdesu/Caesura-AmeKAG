-- =============================================================================
--  Caesura (AmeKAG) — Galgame Demo Entry Point
--  Loads and runs demo/galgame_demo.ks via KAG runner + engine loop callbacks.
--  Place at: demo/entry.lua
--  To use: change config.lua entry_script to "demo/entry.lua"
-- =============================================================================

local kag_runner = require("kag_runner")
local layers = require("layers")

-- ── Start the KAG demo ──────────────────────────────────────────────────────

local started = kag_runner.start("demo/galgame_demo.ks")
if not started then
    print("[Demo Entry] FATAL: Failed to start galgame_demo.ks")
    print("[Demo Entry] Check that demo/galgame_demo.ks exists and has valid syntax.")
    return
end

-- ── Engine loop callbacks ────────────────────────────────────────────────────

--- engine_update(dt) — called every frame by Engine::run()
function engine_update(dt)
    kag_runner.update(dt or 0.016)
end

--- engine_render() — called every frame after engine_update
function engine_render()
    layers.render()
end

--- _KAG_onClick() — called when mouse button is pressed with KAG input focus
function _KAG_onClick()
    kag_runner.on_click()
end

print("[Demo Entry] Galgame demo entry loaded. KAG+Lua hybrid scripting active.")
print("[Demo Entry] Asset paths: assets/bg/ assets/fg/ assets/bgm/ assets/se/ assets/voice/")
