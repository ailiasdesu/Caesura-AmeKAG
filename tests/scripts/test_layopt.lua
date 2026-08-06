-- test_layopt.lua — [layopt]/[layfade]/[cl] contracts (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")
local schema = require("kag.schema")

-- layopt schema: opacity 0..1, visible boolean
local lo = schema.coerce("layopt", { opacity = "5", visible = "true" }, {})
check("layopt opacity clamped", lo.opacity == 1.0)
check("layopt visible kept", lo.visible == true)
local lo2 = schema.coerce("layopt", { visible = "false" }, {})
check("layopt visible false", lo2.visible == false)

-- fadeout schema (layfade's opacity path): 0..1 + time 0..30000
local fo = schema.coerce("fadeout", { opacity = "99", time = "-1" }, {})
check("fadeout opacity clamped", fo.opacity == 1.0)
check("fadeout time clamped", fo.time == 0)

-- handlers registered
check("layopt registered", type(KAG.layopt) == "function")
check("layfade registered", type(KAG.layfade) == "function")
check("cl registered", type(KAG.cl) == "function")

-- source-level: layfade defends missing layers; cl re-applies textbox
local f = assert(io.open("scripts/kag/commands/layer.lua", "r"))
local src = f:read("*a")
f:close()
check("layfade missing-layer guard", src:find("layfade: layer not found", 1, true) ~= nil)
check("cl textbox reapply", src:find("if Text.textbox then Text.textbox(ctx, style) end", 1, true) ~= nil)
check("cl clears bg dedup", src:find("if ctx.layers then ctx.layers.bg = nil end", 1, true) ~= nil)
check("cl clears fg dedup", src:find("if ctx.layers then ctx.layers.fg = nil end", 1, true) ~= nil)

if failed > 0 then os.exit(1) end
print("LAYOPT TESTS DONE")
