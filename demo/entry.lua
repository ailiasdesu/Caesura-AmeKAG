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

function engine_update(dt)
    -- H key: open the backlog overlay ([history] command). HistoryUI.show
    -- yields, and engine_update runs via plain lua_pcall -- wrap in a
    -- coroutine so the yield is legal (mirrors scheduler command flow).
    if _G._GAME_KEY_H then
        _G._GAME_KEY_H = false
        local ctx = _G._CAESURA_CTX
        if ctx and ctx.input_focus ~= "history" then
            local co = coroutine.create(function()
                require("kag.commands.system").history(ctx, {})
            end)
            coroutine.resume(co)
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
