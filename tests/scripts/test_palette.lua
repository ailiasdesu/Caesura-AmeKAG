-- test_palette.lua -- palette 3D-LUT wiring lock (t214)
-- Locks the real-name surface of scripts/palette.lua (load_texture +
-- is_valid_handle + set_postfx('lut3d'); the legacy phantom names
-- set_palette/load_image/is_valid are gone), the Lut3D param shape,
-- clear/unload/active semantics, the headless guard, and the [palette]
-- handler delegation through kag.
package.path = "scripts/?.lua;scripts/?/init.lua;scripts/kag/?.lua;scripts/kag/commands/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local calls = { set_postfx = {}, destroy = {} }
local loaded = {}
local texValid = {}
local nextId = 100
local stubBackend = {
    get_resolution = function() return 1920, 1080 end,
    load_texture = function(path)
        if path == "fail.png" then return 0 end
        if not loaded[path] then
            nextId = nextId + 1
            loaded[path] = nextId
            texValid[tostring(nextId)] = true
        end
        return loaded[path]
    end,
    is_valid_handle = function(hType, h) return texValid[tostring(h)] == true end,
    set_postfx = function(kind, params)
        calls.set_postfx[#calls.set_postfx + 1] = { kind, params }
        return 1
    end,
    is_postfx_supported = function(kind) return kind == "lut3d" end,
    destroy_texture = function(h)
        calls.destroy[#calls.destroy + 1] = h
        texValid[tostring(h)] = nil
    end,
}
-- palette.lua's capability guard reads the GLOBAL backend; the module
-- require("backend") is separate (engine sets both to the same table).
_G.backend = stubBackend
package.loaded["backend"] = stubBackend
package.loaded["rtt"] = { acquire = function() return 0 end, release = function() end }

local palette = require("palette")

-- 1. load: real names, registry entry, error paths
do
    local ok = palette.load("vintage", "assets/lut/vintage.png")
    check("load ok", ok == true)
    check("load uses load_texture", loaded["assets/lut/vintage.png"] ~= nil)
    check("load without id rejected", palette.load("", "x.png") == nil)
    check("load without path rejected", palette.load("v2") == nil)
    local ok2 = palette.load("broken", "fail.png")
    check("load invalid texture rejected", ok2 == nil)
end

-- 2. apply: Lut3D param shape + intensity clamp
do
    local ok = palette.apply("vintage", 2.0)
    check("apply ok", ok == true)
    local last = calls.set_postfx[#calls.set_postfx]
    check("apply calls set_postfx kind lut3d", last and last[1] == "lut3d")
    local p = last and last[2] or {}
    local idVintage = loaded["assets/lut/vintage.png"]
    check("lut3d params lutId + clamped intensity/strength + lutSize",
          p.lutId == idVintage and p.intensity == 1.0 and p.strength == 1.0 and p.lutSize == 0)
    check("apply unknown id rejected", palette.apply("nope", 1.0) == nil)
    local n0 = #calls.set_postfx
    palette.apply("nope", 1.0)
    check("unknown apply does not call set_postfx", #calls.set_postfx == n0)
end

-- 3. clear: disables the stage (lutId=0)
do
    palette.clear()
    local last = calls.set_postfx[#calls.set_postfx]
    check("clear sends lutId=0", last and last[1] == "lut3d" and last[2].lutId == 0)
end

-- 4. unload: clears first when active, then destroys the texture
do
    palette.apply("vintage", 0.5)
    local ok = palette.unload("vintage")
    check("unload ok", ok == true)
    check("unload destroyed the texture", calls.destroy[#calls.destroy] == loaded["assets/lut/vintage.png"])
    check("unload cleared the active stage",
          calls.set_postfx[#calls.set_postfx] and calls.set_postfx[#calls.set_postfx][2].lutId == 0)
    check("unload unknown rejected", palette.unload("ghost") == nil)
end

-- 5. day/night/toggle via the real-name chain
do
    palette.set_day_mode()
    check("day mode clears", palette.get_mode() == "day"
          and calls.set_postfx[#calls.set_postfx][2].lutId == 0)
    local okNight = palette.set_night_mode()
    check("night mode loads + applies", okNight == true and palette.get_mode() == "night")
    local last = calls.set_postfx[#calls.set_postfx]
    check("night apply lutId is the night texture", last and last[1] == "lut3d" and last[2].lutId == loaded["assets/lut/night.png"])
    check("toggle switches back to day", palette.toggle_mode() == "day")
    check("toggle second time goes night", palette.toggle_mode() == "night")
end

-- 6. guard: missing set_postfx surface -> visible no-op, no crash
do
    local saved = package.loaded["backend"]
    local noPostfx = {}
    for k, v in pairs(saved) do noPostfx[k] = v end
    noPostfx.set_postfx = nil
    noPostfx.is_postfx_supported = nil
    -- palette.lua reads the GLOBAL backend for its capability guard.
    local gb = _G.backend
    _G.backend = noPostfx
    local ok = palette.load("g", "x.png")
    check("guard load returns false (no-op)", ok == false)
    local okA = palette.apply("g", 1.0)
    check("guard apply returns false", okA == false)
    _G.backend = gb
    package.loaded["backend"] = saved
end

-- 7. [palette] handler integration (VFXCommands.palette) + schema
do
    local KAG = require("kag")
    check("palette registered in kag", type(KAG.palette) == "function")
    local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
    local okH = pcall(KAG.palette, ctx, { effect = "apply", id = "sd", path = "assets/lut/sd.png", intensity = 0.3 })
    check("palette handler runs ok", okH == true)
    local last = calls.set_postfx[#calls.set_postfx]
    check("handler applied via lut3d", last and last[1] == "lut3d" and last[2].intensity == 0.3)
    local S = require("kag.schema")
    local c = S.coerce("palette", { effect = "apply", intensity = "0.5" }, {})
    check("palette schema coerces", c.effect == "apply" and c.intensity == 0.5)
end

if failed > 0 then os.exit(1) end
print("PALETTE TESTS DONE (" .. passed .. " passed)")
