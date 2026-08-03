-- Title menu tests: navigation + action mapping (no GPU).
local check = function(name, cond)
    if cond then print("PASS " .. name) else print("FAIL " .. name) end
    results = results or {}
    results[#results + 1] = cond
end

package.path = "scripts/?.lua;scripts/?/init.lua;" .. package.path
package.preload["backend"] = function()
    return {
        create_solid_texture = function() return { _mock = true } end,
        render_text = function() end,
        get_input_focus = function() return "kag" end,
    }
end
package.loaded["backend"] = nil

local TitleMenu = require("title_menu")
local ctx = { f = {}, sf = {} }

-- Drive the menu coroutine: press Down then Enter -> should return "load"
local co = coroutine.create(function() return TitleMenu.show(ctx) end)
local ok1 = coroutine.resume(co)  -- first frame renders + yields
check("menu first frame ok", ok1)
-- Down: move cursor to item 2 (load_game)
_G._GAME_KEY_DOWN = true
local ok2 = coroutine.resume(co)
check("down consumed", ok2 and _G._GAME_KEY_DOWN == false)
-- Enter: confirm -> returns "load"
_G._GAME_KEY_ENTER = true
local ok3, action = coroutine.resume(co)
check("enter returns load", ok3 and action == "load")

-- Esc dismisses
co = coroutine.create(function() return TitleMenu.show(ctx) end)
coroutine.resume(co)
_G._GAME_KEY_ESC = true
local ok4, act4 = coroutine.resume(co)
check("esc returns nil", ok4 and act4 == nil)

-- Extended menu: gallery/music items exist and route correctly
do
    local src = io.open("scripts/title_menu.lua", "r"):read("*a")
    check("menu has gallery item", src:find('"gallery"', 1, true) ~= nil)
    check("menu has music item", src:find('"music"', 1, true) ~= nil)
    local entry_src = io.open("scripts/title_demo_entry.lua", "r"):read("*a")
    check("entry routes gallery", entry_src:find('action == "gallery"', 1, true) ~= nil)
    check("entry routes music", entry_src:find('action == "music"', 1, true) ~= nil)
end

-- Exit non-zero on any failure so the harness reports red
local failed = 0
for _, ok in ipairs(results or {}) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("TITLE MENU TESTS DONE")
