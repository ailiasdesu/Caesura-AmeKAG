-- test_four_remaining.lua — br/close/hr/vib coverage (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")
local tokenizer = require("tokenizer")
local scheduler = require("scheduler")

-- [br] delegates to [l] (line break)
local l_calls = 0
local real_l = KAG.l
KAG.l = function() l_calls = l_calls + 1 end
local ok = pcall(KAG.br, { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }, {})
KAG.l = real_l
check("br delegates to l", ok and l_calls == 1)

-- [hr] is a decorative no-op (never crashes)
local ok2 = pcall(KAG.hr, { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }, {})
check("hr no-op safe", ok2)

-- [close] returns the stop signal
local ctxC = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
local r = KAG.close(ctxC, {})
check("close returns stop", r == "stop")

-- [vib] schema clamps (time <= 30000, intensity <= 50)
local schema = require("kag.schema")
local c = schema.coerce("vib", { time = "999999", intensity = "999" }, {})
check("vib clamped", c.time == 30000 and c.intensity == 50)
-- vib handler no-crash (no message layer -> early return)
local ok3 = pcall(KAG.vib, { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }, { time = 100, intensity = 3 })
check("vib no layer no crash", ok3)

-- end-to-end parse shapes
check("br parses", tokenizer.parse("[br]")[1].cmd == "br")
check("hr parses", tokenizer.parse("[hr]")[1].cmd == "hr")
check("close parses", tokenizer.parse("[close]")[1].cmd == "close")
check("vib parses", tokenizer.parse("[vib time=200]")[1].cmd == "vib")

-- KAG3 legacy gaps (audit): endtag/endform no-op safe, g -> bg
check("endtag registered", type(KAG.endtag) == "function")
local okE = pcall(KAG.endtag, {}, {})
check("endtag no-op safe", okE)
check("endform registered", type(KAG.endform) == "function")
local okF = pcall(KAG.endform, {}, {})
check("endform no-op safe", okF)
check("g registered", type(KAG.g) == "function")
local toksG = tokenizer.parse('[g storage="bg.png"]')
check("g parses with storage", toksG[1].cmd == "g"
      and toksG[1].params[1][2] == "bg.png")
-- end-to-end: g routes to bg and registers the layer (review nit)
local layer_backup = package.loaded["layers"]
local created = {}
package.loaded["layers"] = {
    Type = { LAYER_BASE = 0 },
    get_or_create_layer = function(name) created[#created + 1] = name
        return { name = name } end,
    set_layer_image = function() end,
    set_layer_visible = function() end,
    set_z = function() end,
    ensure = function() end,
    get_layer = function() return nil end,
}
local ctxG = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
local okG = pcall(KAG.g, ctxG, { storage = "bg.png" })
package.loaded["layers"] = layer_backup
check("g routes to bg layer", okG and created[1] == "bg"
      and ctxG.layers and ctxG.layers.bg == "bg.png")

if failed > 0 then os.exit(1) end
print("FOUR CMDS TESTS DONE")
