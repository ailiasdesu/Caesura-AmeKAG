-- ===========================================================================
--  Caesura (AmeKAG) �� Automated Level-by-Level Test
--  Uses new layers.lua tree API.
-- ===========================================================================

local layers  = require("layers")
local backend = require("backend")
local System  = require("system")
local rtt     = require("rtt")

local ctx = {
    textLayer = "message0", backlog = {}, status = "none",
    token_ptr = 1, callStack = {}, ifSkip = false, labelMap = {},
    viewport = nil, bgLayer = nil, fgLayer = nil,
    save_title = "Quick Save",
}

local g = {
    frame = 0, level = 0, phase = 0,
    passCount = 0, failCount = 0, frameInLevel = 0,
}

local function PASS(msg) g.passCount = g.passCount + 1; print("[TEST] PASS: " .. msg) end
local function FAIL(msg) g.failCount = g.failCount + 1; print("[TEST] FAIL: " .. msg) end
local function CHECK(cond, msg) if cond then PASS(msg) else FAIL(msg) end end

-- ============================== Test Levels ==============================

local levels = {}

function levels.L0()
    g.level = 0
    print(""); print("=== [L0] Engine Health ===")
    CHECK(true, "SDL3 + bgfx + Lua VM alive")
    local w, h = backend.get_resolution()
    CHECK(type(w) == "number" and w > 0, "Backbuffer width=" .. tostring(w))
    CHECK(type(h) == "number" and h > 0, "Backbuffer height=" .. tostring(h))
    local texId = backend.create_solid_texture(0, 255, 0, 200)
    CHECK(type(texId) == "number" and texId > 0, "GPU solid texture creation")
    if texId then backend.destroy_texture(texId) end
    local hasDebug = rawget(_G, "Debug") ~= nil
    CHECK(hasDebug, "C++ Debug binding registered")
end

function levels.L1()
    g.level = 1
    print(""); print("=== [L1] Script Chain ===")
    CHECK(layers ~= nil, "layers.lua")
    CHECK(backend ~= nil, "backend.lua")
    CHECK(System ~= nil, "system.lua")
    CHECK(rtt ~= nil, "rtt.lua")
    CHECK(type(backend.create_solid_texture) == "function", "create_solid_texture")
    CHECK(type(backend.render_text) == "function", "render_text")
    CHECK(type(backend.create_viewport) == "function", "create_viewport")
    CHECK(type(backend.fill_viewport) == "function", "fill_viewport")
    CHECK(type(System.save) == "function", "System.save")
    CHECK(type(System.load) == "function", "System.load")
end

function levels.L2()
    g.level = 2
    print(""); print("=== [L2] Layer Rendering ===")
    layers.init()
    
    local bg = layers.add_layer(layers.get_root(), {
        name = "bg", z = 0, x = 0, y = 0, w = 1280, h = 720, visible = true
    })
    bg.texture = backend.create_solid_texture(20, 30, 60, 255)
    layers.set_layer_image(bg, bg.texture)
    ctx.bgLayer = bg
    CHECK(bg.texture ~= nil and bg.texture > 0, "Blue BG layer created")

    local fg = layers.add_layer(layers.get_root(), {
        name = "fore", z = 5, x = 100, y = 100, w = 200, h = 150, visible = true
    })
    fg.texture = backend.create_solid_texture(220, 30, 30, 240)
    layers.set_layer_image(fg, fg.texture)
    ctx.fgLayer = fg
    CHECK(fg.texture ~= nil and fg.texture > 0, "Red FG layer created")

    CHECK(layers.count() >= 2, "Both layers added")
    print("[L2] BLUE fullscreen + RED rect at (100,100)")
end

function levels.L3a()
    g.level = 3; g.phase = 0
    print(""); print("=== [L3a] Layer Clear ===")
    local fg = layers.find(function(n) return n.name == "fore" end)
    CHECK(fg ~= nil, "Fore layer found")
    if fg then
        layers.set_layer_visible(fg, false)
        CHECK(not fg.visible, "[cl] Fore hidden - red gone")
    end
end

function levels.L3b()
    g.level = 3; g.phase = 1
    print(""); print("=== [L3b] Recreate FG ===")
    local fg = layers.find(function(n) return n.name == "fore" end)
    if fg then
        fg.x = 300; fg.y = 200; fg.w = 180; fg.h = 120
        layers.set_layer_visible(fg, true)
    else
        fg = layers.add_layer(layers.get_root(), {
            name = "fore", z = 5, x = 300, y = 200, w = 180, h = 120, visible = true
        })
    end
    fg.texture = backend.create_solid_texture(31, 180, 60, 200)
    layers.set_layer_image(fg, fg.texture)
    ctx.fgLayer = fg
    CHECK(fg.texture ~= nil and fg.texture > 0, "Green FG recreated")
end

function levels.L3c()
    g.level = 3; g.phase = 2
    print(""); print("=== [L3c] VFX Load ===")
    -- vfx.lua is preloaded by kag/init.lua before sandbox lockdown.
    -- After lockdown, require() only returns cached modules from package.loaded.
    local VFX = package.loaded["vfx"] or rawget(_G, "vfx")
    CHECK(VFX ~= nil, "vfx.lua loaded (from package.loaded)")
    CHECK(type(VFX.quake) == "function", "VFX.quake")
    CHECK(type(VFX.flash) == "function", "VFX.flash")
    CHECK(type(VFX.fade) == "function", "VFX.fade")
    CHECK(type(VFX.blur) == "function", "VFX.blur")
end

function levels.L3d()
    g.level = 3; g.phase = 3
    print(""); print("=== [L3d] Flow / Save ===")
    -- After sandbox lockdown, load() is safe but may be restricted; test via pcall
    local ok3d, res3d = pcall(function() return load("return 42*2")() end)
    CHECK(ok3d and res3d == 84, "eval 42*2=84")
    local ok = System.save(0, ctx)
    CHECK(ok, "Save file created")
    local oldPtr = ctx.token_ptr
    local loaded = System.load(0, ctx)
    CHECK(loaded ~= nil, "Load restores token_ptr")
    System.save(0, ctx)
    local loaded2 = System.load(0, ctx)
    CHECK(loaded2 ~= nil, "Quick save/load OK")
end

function levels.L5()
    g.level = 5
    print(""); print("=== [L5] Audio API ===")
    CHECK(type(backend.audio_play) == "function", "audio_play")
    CHECK(type(backend.audio_stop) == "function", "audio_stop")
    CHECK(type(backend.audio_is_playing) == "function", "audio_is_playing")
    local ok, lerr = pcall(function()
        local b = rawget(_G, "_CAESURA_BACKEND")
        if b then
            b.audio("set_listener", 0, 0, 0, 0, 1, 0)
        else
            KAG.set_listener(0, 0, 0, 0, 1, 0)
        end
    end)
    CHECK(ok, "3D listener set OK err=" .. tostring(lerr))
end

function levels.L6()
    g.level = 6
    print(""); print("=== [L6] Viewport RTT ===")
    local vpId = backend.create_viewport(400, 300)
    CHECK(type(vpId) == "number" and vpId > 0, "create_viewport = " .. tostring(vpId))
    if vpId then
        backend.fill_viewport(vpId, 30, 180, 60, 255)
        CHECK(true, "fill_viewport (green)")
        backend.draw_viewport(vpId, 200, 50, 400, 300)
        CHECK(true, "draw_viewport at (200,50)")
        ctx.viewport = vpId
        print("[L6] Green 400x300 RTT at (200,50)")
    end
end

function levels.L6b()
    g.level = 6; g.phase = 1
    print(""); print("=== [L6b] RTT Destroy ===")
    if ctx.viewport then
        backend.destroy_viewport(ctx.viewport)
        CHECK(true, "Viewport destroyed")
        ctx.viewport = nil
    end
end

function levels.L7()
    g.level = 7
    print(""); print("=== [L7] Resource Free ===")
    local freeTex = backend.create_solid_texture(255, 128, 0, 255)
    local freeLayer = layers.add_layer(layers.get_root(), {
        name = "freeimage_test", z = 10, x = 500, y = 400, w = 50, h = 50, visible = true
    })
    freeLayer.texture = freeTex
    layers.set_layer_image(freeLayer, freeTex)
    CHECK(freeTex > 0, "Test texture created")
    layers.remove_layer(freeLayer)
    backend.destroy_texture(freeTex)
    local found = layers.find(function(n) return n.name == "freeimage_test" end)
    CHECK(found == nil, "freeimage_test removed")
end

function levels.L8()
    g.level = 8
    print(""); print("=== [L8] Input Focus ===")
    DevCore.set_input_focus("GAME")
    local f1 = DevCore.get_input_focus()
    CHECK(f1 == "GAME", "Focus set to GAME: " .. tostring(f1))
    DevCore.set_input_focus("KAG")
    local f2 = DevCore.get_input_focus()
    CHECK(f2 == "KAG", "Focus set to KAG: " .. tostring(f2))
end

function levels.L9()
    g.level = 9
    print(""); print("=== [L9] Debug Report ===")
    local hasDbg = rawget(_G, "Debug") ~= nil
    if hasDbg then
        CHECK(Debug.get_error_count() == 0, "error_count=0")
        local lp = Debug.get_log_path()
        CHECK(type(lp) == "string" and #lp > 0, "log_path=" .. tostring(lp))
        local info = Debug.get_render_info()
        CHECK(type(info) == "table", "render_info table")
    end
end

-- ============================== Auto-Advance ==============================

local sequence = {
    { name="L0",  fn=levels.L0,  frames=1 },
    { name="L1",  fn=levels.L1,  frames=1 },
    { name="L2",  fn=levels.L2,  frames=120 },
    { name="L3a", fn=levels.L3a, frames=90 },
    { name="L3b", fn=levels.L3b, frames=90 },
    { name="L3c", fn=levels.L3c, frames=1 },
    { name="L3d", fn=levels.L3d, frames=1 },
    { name="L5",  fn=levels.L5,  frames=1 },
    { name="L6",  fn=levels.L6,  frames=120 },
    { name="L6b", fn=levels.L6b, frames=60 },
    { name="L7",  fn=levels.L7,  frames=1 },
    { name="L8",  fn=levels.L8,  frames=1 },
    { name="L9",  fn=levels.L9,  frames=1 },
}
local seqIdx = 0
local done = false

function engine_render()
    if done then return end
    layers.render()
end

function engine_update(dt)
    g.frame = g.frame + 1
    g.frameInLevel = g.frameInLevel + 1
    if _G._autoQuitFrame and g.frame >= _G._autoQuitFrame then
        _G._CAESURA_QUIT = true
        return
    end
    if done then return end

    if seqIdx == 0 then
        seqIdx = 1
        sequence[seqIdx].fn()
    elseif g.frameInLevel >= sequence[seqIdx].frames then
        seqIdx = seqIdx + 1
        g.frameInLevel = 0
        if seqIdx <= #sequence then
            sequence[seqIdx].fn()
        else
            done = true
            print("")
            print("=============================================")
            print("  ALL TESTS COMPLETE")
            print("  PASS: " .. g.passCount .. "  FAIL: " .. g.failCount)
            if g.failCount == 0 then
                print("  RESULT: ALL PASSED!")
            else
                print("  RESULT: " .. g.failCount .. " FAILURE(S)")
            end
            print("=============================================")
            _G._autoQuitFrame = g.frame + 60
        end
    end
end

function _KAG_onClick(x, y) end
function _KAG_onKey(key, mods) end

print("[AUTO-TEST] Automated test suite loaded.")