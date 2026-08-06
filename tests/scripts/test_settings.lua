-- test_settings.lua — settings menu input loop (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local Settings = require("settings")
local real_backend = _G._CAESURA_BACKEND
_G._CAESURA_BACKEND = { render = function() return true end,
    platform = function(cmd)
        if cmd == "get_resolution" then return 1280, 720 end
        if cmd == "set_input_focus" then return true end
        return true end }
local layers_backup = package.loaded["layers"]
package.loaded["layers"] = { ensure = function() return { visible = true } end,
    find = function() return nil end, set_layer_visible = function() end,
    set_z = function() end }
local ctx = { f = {}, sf = {}, tf = {}, mp = {}, variables = {}, settingsValues = {} }
local co = coroutine.create(function() Settings.show(ctx) end)
coroutine.resume(co)
coroutine.resume(co)
check("settings opens", ctx._settingsActive == true)
-- slider: right at cursor 1 bumps volume_bgm by 5
local before = ctx.settingsValues.volume_bgm
_G._GAME_KEY_RIGHT = true
coroutine.resume(co)
check("slider adjusts", ctx.settingsValues.volume_bgm == (before or 0) + 5)
-- toggle: down to skip_mode (item 5) then right flips
for _ = 1, 4 do
    _G._GAME_KEY_DOWN = true
    coroutine.resume(co)
end
local sm_before = ctx.settingsValues.skip_mode
_G._GAME_KEY_RIGHT = true
coroutine.resume(co)
check("toggle flips", ctx.settingsValues.skip_mode ~= sm_before)
-- ESC closes
_G._GAME_KEY_ESC = true
coroutine.resume(co)
check("esc closes", coroutine.status(co) == "dead" and ctx._settingsActive == false)
package.loaded["layers"] = layers_backup
_G._CAESURA_BACKEND = real_backend

if failed > 0 then os.exit(1) end
print("SETTINGS TESTS DONE")
