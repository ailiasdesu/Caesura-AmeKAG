-- test_volume.lua — [set*volume]/[stop*] family contracts (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")
local schema = require("kag.schema")

-- set*volume schemas: volume 0..1.5, NO default (positional fallback)
for _, cmd in ipairs({ "setbgmvolume", "setsevolume", "setvoicevolume" }) do
    local v = schema.coerce(cmd, { volume = "9" }, {})
    check(cmd .. " clamped", v.volume == 1.5)
    local v2 = schema.coerce(cmd, { volume = "-5" }, {})
    check(cmd .. " min clamped", v2.volume == 0)
    local v3 = schema.coerce(cmd, {}, {})
    check(cmd .. " no default", v3.volume == nil)
end

-- stopbgm schema: fadeout 0..30000
local sb = schema.coerce("stopbgm", { fadeout = "99999" }, {})
check("stopbgm fadeout clamped", sb.fadeout == 30000)

-- handlers registered
check("setbgmvolume registered", type(KAG.setbgmvolume) == "function")
check("setvoicevolume registered", type(KAG.setvoicevolume) == "function")
check("stopbgm registered", type(KAG.stopbgm) == "function")

-- source-level: clampVolume double-guard (schema + handler clamp)
local f = assert(io.open("scripts/kag/commands/audio.lua", "r"))
local src = f:read("*a")
f:close()
check("clampVolume guard", src:find("math.min(1.5, math.max(0, v or 1.0))", 1, true) ~= nil)
check("positional fallback", src:find("tonumber(params[1]) or 1.0", 1, true) ~= nil)

if failed > 0 then os.exit(1) end
print("VOLUME TESTS DONE")
