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
-- the ONLY create_solid_texture call site in vfx.lua feeds the clamped
-- r/g/b (review warn: the earlier (.-)end capture truncated at the
-- first 'end' and plain=true made %( a literal -- vacuous. This form
-- checks the real call site with a proper pattern.)
local site = src:match("create_solid_texture%(([^)]*)%)")
check("solid texture call exists", site ~= nil)
check("feed is clamped vars", site == "r, g, b, 255")


-- ═══════════════════════════════════════════════════════════════════════
--  [palette] command: routes to scripts/palette.lua (LUT color grading).
--  palette.lua drives the backend through the GLOBAL `backend`; we mock
--  _G.backend with a recorder and assert the palette handler forwards
--  params + defaults to palette.load -> backend.load_texture and
--  palette.apply -> backend.set_postfx("lut3d", ...).
-- ═══════════════════════════════════════════════════════════════════════
local vfxLog = {}

-- Backend mock: records every palette-directed call. __index returns a
-- no-op for anything vfx.lua/layers touch at require time (we do not run
-- the blocking effect loops in this file).
local backendMock = setmetatable({
    load_texture = function(path)
        vfxLog[#vfxLog + 1] = { "load_texture", path }
        return 42
    end,
    is_valid_handle = function(kind, h)
        vfxLog[#vfxLog + 1] = { "is_valid_handle", kind, h }
        return kind == 0 and h ~= nil and h > 0
    end,
    set_postfx = function(kind, params)
        vfxLog[#vfxLog + 1] = { "set_postfx", kind, params }
        return true
    end,
    destroy_texture = function(handle)
        vfxLog[#vfxLog + 1] = { "destroy_texture", handle }
    end,
}, { __index = function() return function() end end })

local savedBackend = package.loaded["backend"]
local savedLayers = package.loaded["layers"]
package.loaded["backend"] = backendMock
_G.backend = backendMock
package.loaded["layers"] = { forEach = function() end, get_layer = function() return nil end, find = function() return nil end }

local vfxSchema = require("kag.schema")
local VFXCommands = require("kag.commands.vfx")

check("palette handler exists", type(VFXCommands.palette) == "function")
check("vibrate handler exists", type(VFXCommands.vibrate) == "function")
check("palette schema migrated", vfxSchema.isMigrated("palette"))
check("vibrate schema migrated", vfxSchema.isMigrated("vibrate"))

-- palette schema coercion (typed + clamped)
do
    local p1 = vfxSchema.coerce("palette", { effect = "apply", id = "lut1", path = "assets/lut/a.png", intensity = "0.5" }, {})
    check("palette effect coerced", p1.effect == "apply")
    check("palette intensity coerced", p1.intensity == 0.5)
    check("palette intensity default", (vfxSchema.coerce("palette", { id = "x" }, {})).intensity == 1.0)
    local p2 = vfxSchema.coerce("palette", { effect = "apply", intensity = "2.5" }, {})
    check("palette intensity clamped to 1.0", p2.intensity == 1.0)
end

-- palette apply with path -> load_image(path) then set_palette(handle, intensity)
do
    local ctx = {}
    vfxLog = {}
    VFXCommands.palette(ctx, vfxSchema.coerce("palette", {
        effect = "apply", id = "lut1", path = "assets/lut/a.png", intensity = "0.5",
    }, {}))
    local oks, sps = false, false
    for _, c in ipairs(vfxLog) do
        if c[1] == "load_texture" and c[2] == "assets/lut/a.png" then oks = true end
        if c[1] == "set_postfx" and c[2] == "lut3d" and c[3] and c[3].lutId == 42 and c[3].strength == 0.5 then sps = true end
    end
    check("palette apply loads LUT image", oks)
    check("palette apply applies LUT (handle, intensity)", sps)
end

-- palette apply without a path: re-apply a previously-loaded id
do
    local ctx = {}
    vfxLog = {}
    VFXCommands.palette(ctx, { effect = "apply", id = "lut1", path = "assets/lut/a.png", intensity = 1.0 })
    vfxLog = {}
    VFXCommands.palette(ctx, { effect = "apply", id = "lut1", intensity = 0.8 })
    local sps = false
    for _, c in ipairs(vfxLog) do
        if c[1] == "set_postfx" and c[2] == "lut3d" and c[3] and c[3].lutId == 42 and c[3].strength == 0.8 then sps = true end
    end
    check("palette apply id-only re-applies", sps)
end

-- palette clear -> set_palette(nil, 0, 0)
do
    local ctx = {}
    vfxLog = {}
    VFXCommands.palette(ctx, { effect = "clear" })
    local clr = false
    for _, c in ipairs(vfxLog) do
        if c[1] == "set_postfx" and c[2] == "lut3d" and c[3] and c[3].lutId == 0 and c[3].intensity == 0 then clr = true end
    end
    check("palette clear disables LUT", clr)
end

-- palette day/night/toggle modes via palette.get_mode()
do
    local ctx = {}
    VFXCommands.palette(ctx, { effect = "day" })
    check("palette day mode", require("palette").get_mode() == "day")
    VFXCommands.palette(ctx, { effect = "night" })
    check("palette night mode", require("palette").get_mode() == "night")
    VFXCommands.palette(ctx, { effect = "toggle" })
    check("palette toggle -> day", require("palette").get_mode() == "day")
end

-- palette unload -> destroy_texture
do
    local ctx = {}
    vfxLog = {}
    VFXCommands.palette(ctx, { effect = "unload", id = "lut1" })
    local unf = false
    for _, c in ipairs(vfxLog) do
        if c[1] == "destroy_texture" and c[2] == 42 then unf = true end
    end
    check("palette unload destroys LUT texture", unf)
end

-- [vibrate] alias -> [vib]: verbatim param forwarding to transition.vib
do
    local vibCalls = {}
    local fakeTrans = {
        vib = function(ctx, params)
            vibCalls[#vibCalls + 1] = { ctx = ctx, params = params }
        end,
    }
    local savedTrans = package.loaded["kag.commands.transition"]
    package.loaded["kag.commands.transition"] = fakeTrans
    local ctx = { current_scene = "t.ks" }
    local pc = { time = "500", intensity = "7" }
    VFXCommands.vibrate(ctx, vfxSchema.coerce("vibrate", pc, ctx))
    check("vibrate forwards to vib", #vibCalls == 1)
    check("vibrate forwards time", vibCalls[1] and vibCalls[1].params.time == 500)
    check("vibrate forwards intensity", vibCalls[1] and vibCalls[1].params.intensity == 7)
    check("vibrate forwards ctx", vibCalls[1] and vibCalls[1].ctx == ctx)
    -- intensity clamp parity with vib
    local vv = vfxSchema.coerce("vibrate", { time = "10", intensity = "999" }, ctx)
    check("vibrate intensity clamped", vv.intensity == 50)
    package.loaded["kag.commands.transition"] = savedTrans
end

-- restore backend/layers resolution
package.loaded["backend"] = savedBackend
_G.backend = nil
package.loaded["layers"] = savedLayers

if failed > 0 then os.exit(1) end
print("VFX CLAMP TESTS DONE")
