-- test_fadeout.lua — [fadeout] layfade delegation (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")
local tokenizer = require("tokenizer")
check("fadeout registered", type(KAG.fadeout) == "function")
local t = tokenizer.parse('[fadeout layer="bg" time=500]')
check("fadeout parses", t[1].cmd == "fadeout" and t[1].params[1][2] == "bg")

-- behavior: delegates to layfade with opacity 0 (default fade-out)
local Layer = require("kag.commands.layer")
local calls = {}
local real_layfade = Layer.layfade
Layer.layfade = function(ctx, p)
    calls[#calls + 1] = p
end
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
local ok = pcall(KAG.fadeout, ctx, { layer = "bg", time = 700 })
Layer.layfade = real_layfade
check("fadeout delegates", ok and calls[1]
      and calls[1].layer == "bg" and calls[1].opacity == 0 and calls[1].time == 700)

-- explicit opacity passes through
calls = {}
Layer.layfade = function(ctx, p) calls[#calls + 1] = p end
pcall(KAG.fadeout, ctx, { layer = "fg", opacity = 0.3 })
Layer.layfade = real_layfade
check("fadeout explicit opacity", calls[1] and calls[1].opacity == 0.3)

-- schema contract intact (opacity clamp)
local schema = require("kag.schema")
local c = schema.coerce("fadeout", { opacity = "9", time = "1" }, {})
check("fadeout opacity clamped", c.opacity == 1.0)

if failed > 0 then os.exit(1) end
print("FADEOUT TESTS DONE")
