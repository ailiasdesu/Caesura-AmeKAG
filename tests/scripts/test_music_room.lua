-- test_music_room.lua — [music_room] input loop (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local MusicRoom = require("music_room")
local plays, favs = {}, {}
local real_scan = MusicRoom.scan
MusicRoom.scan = function()
    return { { id = "t1", name = "Track One", path = "a.ogg" },
             { id = "t2", name = "Track Two", path = "b.ogg" } }
end
MusicRoom.play = function(id) plays[#plays + 1] = id return true end
MusicRoom.favorite = function(id) favs[#favs + 1] = id end
MusicRoom.hide = function() end
local real_backend = _G._CAESURA_BACKEND
_G._CAESURA_BACKEND = { render = function() return true end,
    platform = function(cmd)
        if cmd == "get_resolution" then return 1280, 720 end
        return true end }
local layers_backup = package.loaded["layers"]
package.loaded["layers"] = { ensure = function() return { visible = true } end,
    set_layer_visible = function() end, set_z = function() end }
local ctx = { f = {}, sf = {}, tf = {}, mp = {}, variables = {},
    unlockedMusic = { t1 = true } }
local co = coroutine.create(function() MusicRoom.show(ctx) end)
coroutine.resume(co)
coroutine.resume(co)
-- DOWN to track 2 (LOCKED): Enter must refuse, F must favorite
_G._GAME_KEY_DOWN = true
coroutine.resume(co)
_G._GAME_KEY_ENTER = true
coroutine.resume(co)
check("locked track refused", #plays == 0)
_G._GAME_KEY_F = true
coroutine.resume(co)
check("favorite works on locked", favs[1] == "t2")
-- UP to track 1 (unlocked): Enter plays
_G._GAME_KEY_UP = true
coroutine.resume(co)
_G._GAME_KEY_ENTER = true
coroutine.resume(co)
check("unlocked track plays", plays[1] == "t1")
-- ESC exits
_G._GAME_KEY_ESC = true
coroutine.resume(co)
check("esc exits", coroutine.status(co) == "dead")
package.loaded["layers"] = layers_backup
_G._CAESURA_BACKEND = real_backend
MusicRoom.scan = real_scan

if failed > 0 then os.exit(1) end
print("MUSIC ROOM TESTS DONE")
