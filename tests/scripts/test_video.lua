-- test_video.lua — [video]/[stopvideo] contracts (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local Video = package.loaded["kag.commands.video"] or require("kag.commands.video")
check("video handler", type(Video.video) == "function")
check("stopvideo handler", type(Video.stopvideo) == "function")

local KAG = require("kag")
check("registered on KAG", type(KAG.video) == "function"
      and type(KAG.stopvideo) == "function")

-- source: the 60s cap bounds the wait loop (same as waitsound)
local f = assert(io.open("scripts/kag/commands/video.lua", "r"))
local src = f:read("*a")
f:close()
check("wait capped", src:find("elapsed < 60000", 1, true) ~= nil)
check("cancel honored", src:find("not ct.cancelled and elapsed < 60000", 1, true) ~= nil)
-- loop is schema-boolean (no string dead branches)
check("loop boolean", src:find("params.loop == true", 1, true) ~= nil
      and not src:find('loop == "false"', 1, true))
-- stopvideo registered + schema typed (volume clamp)
local schema = require("kag.schema")
local coerced = schema.coerce("video", { file = "op.avi", volume = "9" })
check("volume clamped", coerced.volume == 1.5)

-- string-param sweep (audit): loop="true" (direct call, no coerce)
-- must loop; non-numeric opacity on fadeout must not raise
local Video2 = package.loaded["kag.commands.video"] or require("kag.commands.video")
local v_calls = {}
local be2 = _G._CAESURA_BACKEND
_G._CAESURA_BACKEND = { render = function(cmd, ...)
    if cmd == "video_play" then v_calls[#v_calls + 1] = { ... } end
    return true end }
local ctxV = { f = {}, tf = {}, sf = {}, mp = {}, variables = {}, viewport = { width = 1280, height = 720 } }
local okV = pcall(Video2.video, ctxV, { file = "a.mpg", loop = "true", time = 1 })
check("loop string tolerated", okV and v_calls[1] and v_calls[1][2].loop == true)
_G._CAESURA_BACKEND = be2

local okF = pcall(KAG.fadeout, { f = {}, tf = {}, sf = {}, mp = {}, variables = {} },
                  { layer = "bg", opacity = "abc", time = 1 })
check("fadeout bad opacity no raise", okF)
_G._CAESURA_BACKEND = be2

if failed > 0 then os.exit(1) end
print("VIDEO TESTS DONE")
