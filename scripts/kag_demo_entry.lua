-- =============================================================================
--  Caesura (AmeKAG) — kag_demo_entry.lua
--  KAG demo entry point. Loads and runs a .ks script via the KAG runner,
--  wiring the coroutine bridge into the engine's per-frame callbacks.
--
--  To activate: change config.entry_script = "kag_demo_entry.lua" in config.lua
--  No C++ rebuild needed — this is pure Lua, loaded via luaL_dofile.
-- =============================================================================

local kag_runner = require("kag_runner")
local layers = require("layers")

-- Start the demo story
kag_runner.start("scripts/demo_story.ks")

-- ── Engine update callback (called each frame by C++ Engine::run) ────────────

function engine_update(dt)
    kag_runner.update(dt)
end

-- ── Engine render callback (called each frame after update) ──────────────────
-- KAG commands ([bg], [fg], [ch], etc.) manipulate layer state; this call
-- submits the layer tree to the GPU render pipeline.

function engine_render()
    layers.render()
    kag_runner.render()
end

-- ── Input callback (called by C++ processEvents on mouse click, KAG focus) ───

function _KAG_onClick()
    kag_runner.on_click()
end

print("[KAG Demo] Entry loaded — running demo_story.ks")
