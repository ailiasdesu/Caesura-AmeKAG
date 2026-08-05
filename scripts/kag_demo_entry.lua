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
local _toast = pcall(require, "toast") and require("toast") or nil

-- Ctrl hold-to-skip: D9.6 dispatches _KAG_onCtrlDown/Up from Engine.cpp
-- (key-repeat guarded); wire them so Ctrl skips while held and releases
-- restore the previous skip mode -- standard VN hold-skip behavior.
local _prevSkipMode = nil

function _KAG_onCtrlDown()
    local ctx = _G._CAESURA_CTX
    if not ctx then return end
    if _prevSkipMode == nil then _prevSkipMode = ctx.skip_mode end
    ctx.skip_mode = true
end

function _KAG_onCtrlUp()
    local ctx = _G._CAESURA_CTX
    if not ctx then return end
    ctx.skip_mode = _prevSkipMode
    _prevSkipMode = nil
end

function engine_update(dt)
    -- F4: toggle the developer HUD (perf overlay)
    if _G._GAME_KEY_F4 then
        _G._GAME_KEY_F4 = false
        require("dev_hud").toggle()
    end
    local _devHud = pcall(require, "dev_hud") and require("dev_hud") or nil
    if _devHud then
        pcall(function() _devHud.update(dt * 1000) end)
    end
    if _toast then
        pcall(function() _toast.update(dt) end)
    end
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
            -- The overlay may have set input_focus="history" before dying;
            -- reset it or clicks/H re-open stay deadlocked (soft-lock).
            local ctx = _G._CAESURA_CTX
            if ctx then
                ctx.input_focus = "kag"
                pcall(function()
                    require("history_ui")._hideAll(ctx)
                end)
            end
        elseif coroutine.status(history_co) == "dead" then
            history_co = nil
        end
    end
    -- V key: replay the current line's voice (backlog latest entry).
    if _G._GAME_KEY_V then
        _G._GAME_KEY_V = false
        local ctx = _G._CAESURA_CTX
        if ctx and ctx.input_focus ~= "history" and ctx.backlog and ctx.backlog[1] then
            local e = ctx.backlog[1]
            local v = e.voice
            if type(v) == "string" and #v > 0
                and not v:find("..", 1, true)
                and (v:match("%.ogg$") or v:match("%.wav$")) then
                pcall(function() backend.audio_play("voice", v) end)
            end
        end
    end
    -- A key: toggle auto mode in-game (Ren'Py-style quick toggle).
    if _G._GAME_KEY_A == true then
        _G._GAME_KEY_A = false
        local ctx = _G._CAESURA_CTX
        if ctx then
            ctx.auto_mode = not ctx.auto_mode
            if _toast then
                pcall(function() _toast.show(ctx.auto_mode and "Auto ON" or "Auto OFF") end)
            end
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
