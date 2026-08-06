-- test_fade_audio.lua — [fadebgm]/[xfadebgm] contracts (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")
local schema = require("kag.schema")

-- fadebgm schema: volume 0..1.5, time 0..30000
local fb = schema.coerce("fadebgm", { volume = "9", time = "99999" }, {})
check("fadebgm volume clamped", fb.volume == 1.5)
check("fadebgm time clamped", fb.time == 30000)

-- xfadebgm schema: time clamp + require file
local xf = schema.coerce("xfadebgm", { storage = "next.ogg", time = "0" }, {})
check("xfadebgm storage kept", xf.storage == "next.ogg")
check("xfadebgm time kept", xf.time == 0)

-- handlers registered
check("fadebgm registered", type(KAG.fadebgm) == "function")
check("xfadebgm registered", type(KAG.xfadebgm) == "function")

-- source-level: ms -> seconds conversion + backend delegation
local f = assert(io.open("scripts/kag/commands/audio.lua", "r"))
local src = f:read("*a")
f:close()
check("fadebgm ms conversion", src:find("time / 1000.0", 1, true) ~= nil)
check("xfadebgm delegates", src:find('backend.audio_xfade("bgm", file, time / 1000.0)', 1, true) ~= nil)
check("fadebgm delegates", src:find('backend.audio_fade_volume("bgm", target, time / 1000.0)', 1, true) ~= nil)

if failed > 0 then os.exit(1) end
print("FADE AUDIO TESTS DONE")
