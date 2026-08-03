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

-- History overlay coroutine: HistoryUI.show yields once per frame, so the
-- wrapper must be resumed every frame until it finishes (a single resume
-- would render one frame then freeze with ctx.input_focus stuck on
-- "history", deadlocking clicks/skip/auto).
local history_co = nil

function engine_update(dt)
    -- H key: open the backlog overlay ([history] command).
    if _G._GAME_KEY_H then
        _G._GAME_KEY_H = false
        local ctx = _G._CAESURA_CTX
        if ctx and ctx.input_focus ~= "history" and not history_co then
            history_co = coroutine.create(function()
                require("kag.commands.system").history(ctx, {})
            end)
        end
    end
    if history_co then
        local ok, err = coroutine.resume(history_co)
        if not ok then
            print("[History] overlay error: " .. tostring(err))
            history_co = nil
        elseif coroutine.status(history_co) == "dead" then
            history_co = nil
        end
    end
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
