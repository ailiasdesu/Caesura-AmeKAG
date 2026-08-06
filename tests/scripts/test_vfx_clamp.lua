-- test_vfx_clamp.lua — vfx flash color clamp (security audit lock)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local f = assert(io.open("scripts/vfx.lua", "r"))
local src = f:read("*a")
f:close()
check("flash clamp helper", src:find("local function clamp_byte", 1, true) ~= nil)
check("flash clamps r", src:find("clamp_byte(params.r or params.red or 255)", 1, true) ~= nil)
check("flash clamps g", src:find("clamp_byte(params.g or params.green or 255)", 1, true) ~= nil)
check("flash clamps b", src:find("clamp_byte(params.b or params.blue or 255)", 1, true) ~= nil)
check("clamp floors", src:find("math.floor(tonumber(v) or 0)", 1, true) ~= nil)
-- no raw tonumber feed into create_solid_texture inside VFX.flash
local flash = src:match("function VFX.flash(.-)end", 1)
check("flash body clamped", flash and not flash:find("create_solid_texture%(r, g, b", 1, true))

if failed > 0 then os.exit(1) end
print("VFX CLAMP TESTS DONE")
