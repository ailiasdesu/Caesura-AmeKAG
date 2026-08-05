-- =============================================================================
--  Caesura (AmeKAG) — title_demo_entry.lua
--  Title-screen demo entry: shows the title menu first, then routes the
--  chosen action (New Game / Load Game / Settings / Exit) into the engine.
--  Pure Lua (no C++ rebuild); activate via config.entry_script.
-- =============================================================================

local kag_runner = require("kag_runner")
local layers = require("layers")

-- Title-menu coroutine: TitleMenu.show yields per frame; resume it every
-- frame until it returns an action (same pattern as the history overlay).
local title_co = nil
local started = false


local _toast = pcall(require, "toast") and require("toast") or nil
local _devHud = pcall(require, "dev_hud") and require("dev_hud") or nil

function engine_update(dt)
    if _devHud then pcall(function() _devHud.update(dt * 1000) end) end
    if _toast then pcall(function() _toast.update(dt) end) end

    -- Drive the title menu until an action is chosen
    if title_co then
        local ok, action = coroutine.resume(title_co)
        if not ok then
            print("[TitleMenu] error: " .. tostring(action))
            title_co = nil
            -- fall back to starting the demo directly
            kag_runner.start("scripts/demo_story.ks")
            started = true
        elseif coroutine.status(title_co) == "dead" then
            title_co = nil
            if action == "new" then
                kag_runner.start("scripts/demo_story.ks")
                started = true
            elseif action == "load" or action == "continue" then
                -- continue = load the autosave (slot 0); plain load picks
                -- the default slot via the saveload menu.
                local ok_load, err = pcall(function()
                    if action == "continue" then
                        require("kag.commands.save").load(_G._CAESURA_CTX, { slot = 0 })
                    else
                        require("kag.commands.save").load(_G._CAESURA_CTX, {})
                    end
                end)
                if not ok_load then
                    print("[TitleMenu] load failed: " .. tostring(err))
                    kag_runner.start("scripts/demo_story.ks")
                end
                started = true
            elseif action == "settings" then
                -- Open the settings overlay from the title screen (modal;
                -- Settings.show drives its own coroutine loop)
                local ctx = _G._CAESURA_CTX
                if ctx then
                    local co = coroutine.create(function()
                        require("settings").show(ctx)
                    end)
                    coroutine.resume(co)
                end
                -- Settings is modal; after it closes, start the demo
                kag_runner.start("scripts/demo_story.ks")
                started = true
            elseif action == "exit" then
                local engine = rawget(_G, "_CAESURA_ENGINE")
                if engine and engine.quit then engine:quit() end
            end
        end
    end

    -- Once the game has started, hand over to the KAG runner
    if started then
        kag_runner.update(dt)
    end
end

function engine_render()
    layers.render()
end

function _KAG_onClick()
    if started then kag_runner.on_click() end
end

-- Begin: show the title menu
local ctx = _G._CAESURA_CTX
if ctx then
    title_co = coroutine.create(function() return require("title_menu").show(ctx) end)
end

print("[TitleDemo] Title screen entry loaded")
