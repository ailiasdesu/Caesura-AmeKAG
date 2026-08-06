-- test_vib_camera.lua — [vib]/[camera] contracts (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")
local schema = require("kag.schema")

-- vib schema: time/intensity/amplitude clamps
local v = schema.coerce("vib", { time = "99999", intensity = "99", amplitude = "-1" }, {})
check("vib time clamped", v.time == 30000)
check("vib intensity clamped", v.intensity == 50)
check("vib amplitude clamped", v.amplitude == 0)

-- camera schema: x/y/time clamps + restore default
local c = schema.coerce("camera", { x = "99999", y = "-5", time = "0" }, {})
check("camera x clamped", c.x == 2000)
check("camera y clamped", c.y == 0)
check("camera time kept", c.time == 0)
local c2 = schema.coerce("camera", {}, {})
check("camera restore default", c2.restore == true)

-- handlers registered
check("vib registered", type(KAG.vib) == "function")
check("camera registered", type(KAG.camera) == "function")

-- source-level: vib restores the base position on completion and the
-- cancel path restores too (Operation <close> semantics)
local f = assert(io.open("scripts/kag/commands/transition.lua", "r"))
local src = f:read("*a")
f:close()
check("vib restores base", src:find("msg.x, msg.y = baseX, baseY", 1, true) ~= nil)
check("camera restore flag", src:find("params.restore", 1, true) ~= nil
      and src:find('restore = { type = "boolean", default = true }', 1, true) ~= nil)

if failed > 0 then os.exit(1) end
print("VIB CAMERA TESTS DONE")
