-- test_alias_bare.lua — KAG3 alias bare-arg consumption (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

-- [se 1] parses with the bare value and reaches playse as file
local tokenizer = require("tokenizer")
local toks = tokenizer.parse("[se 1]")
check("se bare parsed", toks[1].cmd == "se" and toks[1].params[1][2] == "1")

-- [voice 2] / [play 3] likewise
local tv = tokenizer.parse("[voice 2]")
check("voice bare parsed", tv[1].cmd == "voice" and tv[1].params[1][2] == "2")
local tp = tokenizer.parse("[play 3]")
check("play bare parsed", tp[1].cmd == "play" and tp[1].params[1][2] == "3")

-- behavior: KAG.se maps params[1] into playse file
local calls = {}
local KAG = require("kag")
local audio = package.loaded["kag.commands.audio"] or require("kag.commands.audio")
local real_playse = audio.playse
audio.playse = function(ctx, p)
    calls[#calls + 1] = { p.file, p.volume }
end
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    current_scene = "t.ks", token_index = 1 }
local ok = pcall(KAG.se, ctx, { "1" })
check("se routes bare to file", ok and calls[1] and calls[1][1] == "1")
audio.playse = real_playse

-- KAG.play maps params[1] into the bus handler
local real_playbgm = audio.playbgm
local calls2 = {}
audio.playbgm = function(ctx, p) calls2[#calls2 + 1] = p.file end
local ok2 = pcall(KAG.play, ctx, { "3" })
check("play routes bare to file", ok2 and calls2[1] == "3")
audio.playbgm = real_playbgm

if failed > 0 then os.exit(1) end
print("ALIAS BARE TESTS DONE")
