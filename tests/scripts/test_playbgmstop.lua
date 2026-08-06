-- test_playbgmstop.lua — [playbgmstop] path contract (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")
local schema = require("kag.schema")

-- playbgmstop schema: file/volume/fadeout/fadein clamps
local p = schema.coerce("playbgmstop", { volume = "9", fadeout = "99999", fadein = "-1" }, {})
check("pbs volume clamped", p.volume == 1.5)
check("pbs fadeout clamped", p.fadeout == 30000)
check("pbs fadein clamped", p.fadein == 0)

-- handler registered
check("playbgmstop registered", type(KAG.playbgmstop) == "function")

-- source-level: stop-then-play chain, race guard on fadeout, file optional
-- anchored to the playbgmstop function's line range (review nit: the
-- shared substrings also exist in playbgm/stopbgm -- presence alone is
-- not discriminating).
local f = assert(io.open("scripts/kag/commands/audio.lua", "r"))
local src = f:read("*a")
f:close()
local sstart = src:find("function AudioCommands.playbgmstop", 1, true)
local send = src:find("function AudioCommands.fadebgm", sstart or 1, true)
local body = sstart and send and src:sub(sstart, send) or ""
check("pbs body found", #body > 0)
check("stop in pbs body", body:find('backend.audio_stop("bgm"', 1, true) ~= nil)
check("play in pbs body", body:find('backend.audio_play("bgm", file, {', 1, true) ~= nil)
check("race guard in pbs body", body:find("fadeout / 1000.0 + 0.1", 1, true) ~= nil)
check("file optional in pbs body", body:find("if file then", 1, true) ~= nil)

if failed > 0 then os.exit(1) end
print("PLAYBGMSTOP TESTS DONE")
