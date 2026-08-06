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

-- scroll window (review warn): 25 tracks, cursor past the 22-row
-- window must still map to a rendered row; the gold highlight must
-- follow the cursor (review nit: assert the rendered color)
local big = {}
for i = 1, 25 do big[#big + 1] = { id = "t" .. i, name = "T" .. i, path = "p" .. i } end
MusicRoom.scan = function() return big end
local goldRows = {}
_G._CAESURA_BACKEND = { render = function(cmd, ...)
    if cmd == "render_text" then
        local a = { ... }
        if a[4] == 255 and a[5] == 220 and a[6] == 80 then
            goldRows[#goldRows + 1] = a[1]
        end
    end
    return true end,
    platform = function(cmd)
        if cmd == "get_resolution" then return 1280, 720 end
        return true end }
local ctxB = { f = {}, sf = {}, tf = {}, mp = {}, variables = {}, unlockedMusic = {} }
local coB = coroutine.create(function() MusicRoom.show(ctxB) end)
coroutine.resume(coB)
coroutine.resume(coB)
for _ = 1, 24 do
    _G._GAME_KEY_DOWN = true
    coroutine.resume(coB)
end
-- cursor at 25; Enter must pick t25 (unlocked? no -- locked, so no play)
_G._GAME_KEY_ENTER = true
local okB = coroutine.resume(coB)
check("scroll cursor no crash", okB and coroutine.status(coB) == "suspended")
local goldT25 = false
for _, row in ipairs(goldRows) do
    if row:find("T25") then goldT25 = true break end
end
check("gold follows cursor", goldT25)
_G._GAME_KEY_ESC = true
coroutine.resume(coB)
check("scroll esc exits", coroutine.status(coB) == "dead")

if failed > 0 then os.exit(1) end
print("MUSIC ROOM TESTS DONE")
