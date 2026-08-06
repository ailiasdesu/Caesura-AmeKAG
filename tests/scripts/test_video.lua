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

if failed > 0 then os.exit(1) end
print("VIDEO TESTS DONE")
