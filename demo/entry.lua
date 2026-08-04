-- Caesura (AmeKAG) — Galgame Demo Entry Point
local kag_runner = require("kag_runner")
local layers = require("layers")

local function file_exists(path)
    local f = io.open(path, "r")
    if f then f:close(); return true end
    return false
end

local demo_path = nil
for _, p in ipairs({"demo/galgame_demo.ks", "galgame_demo.ks", "../demo/galgame_demo.ks"}) do
    if file_exists(p) then demo_path = p; break end
end

if not demo_path then
    print("[Demo Entry] FATAL: Cannot find galgame_demo.ks")
    return
end

print("[Demo Entry] Loading: " .. demo_path)
local started = kag_runner.start(demo_path)
if not started then
    print("[Demo Entry] FATAL: Failed to start demo")
    return
end

-- History overlay coroutine: HistoryUI.show yields once per frame, so the
-- wrapper must be resumed every frame until it finishes (a single resume
-- would render one frame then freeze with ctx.input_focus stuck on
-- "history", deadlocking clicks/skip/auto).
local history_co = nil

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
            if require("toast") and toast ~= nil then
                pcall(function() toast.show(ctx, ctx.auto_mode and "Auto ON" or "Auto OFF") end)
            end
        end
    end
    kag_runner.update(dt or 0.016)
end

function engine_render()
    layers.render()
end

function _KAG_onClick()
    kag_runner.on_click()
end

print("[Demo Entry] KAG+Lua hybrid scripting active.")
