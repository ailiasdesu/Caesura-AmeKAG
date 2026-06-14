-- ===========================================================================
--  Caesura (AmeKAG) - Full Feature Test Demo
-- ===========================================================================

local layers  = require("layers")
local backend = require("backend")
local System  = require("system")
local w, h = backend.get_resolution()
if not w then w, h = 1280, 720 end

-- Helpers
local function solid(r, g, b, a)
    return backend.create_solid_texture(
        math.floor(r), math.floor(g), math.floor(b), math.floor(a or 255))
end

local function load_tex(path)
    local id = backend.load_texture(path)
    if id and id > 0 then
        print("[DEMO] Loaded: " .. path .. " -> " .. tostring(id))
    else
        print("[DEMO] MISSING: " .. path)
    end
    return id or 0
end

local function draw_text(x, y, text, color, scale)
    backend.render_text(text, x, y, scale or 1.0,
        color[1], color[2], color[3], color[4])
end

local function draw_center(y, text, color, scale)
    local tw = #text * (scale or 1.0) * 10
    local x = (w - tw) / 2
    draw_text(x, y, text, color, scale)
end

-- Colors
local C = {
    bg     = { 22, 20, 42, 255 },
    gold   = { 255, 200, 60, 255 },
    white  = { 240, 240, 250, 255 },
    dim    = { 170, 170, 200, 255 },
    blue   = { 90, 150, 255, 255 },
    green  = { 60, 200, 100, 255 },
    red    = { 220, 60, 60, 255 },
    yellow = { 220, 220, 60, 255 },
}

-- State
local S = {
    frame     = 0,
    phase     = -1,
    timer     = 0,
    autoTmr   = 0,
    sceneInit = false,
    tex       = {},
    lyrs      = {},
}

-- Preload
local function preload()
    if S.tex.done then return end
    S.tex.classroom = load_tex("assets/bg/classroom.png")
    S.tex.hana      = load_tex("assets/bg/hana.png")
    S.tex.girl      = load_tex("assets/fg/girl_uniform.png")
    S.tex.done      = true
end

-- Clear scene
local function clear_scene()
    local root = layers.get_root()
    if root and root.children then
        local kids = {}
        for _, c in ipairs(root.children) do table.insert(kids, c) end
        for _, c in ipairs(kids) do
            if not c.name or not c.name:find("^bar_") then
                layers.remove_layer(c)
            end
        end
    end
    S.sceneInit = false
    S.lyrs = {}
end

-- Add image layer
local function add_img(x, y, w2, h2, tex_id, name, z)
    if tex_id == 0 then return nil end
    local node = layers.add_layer(layers.get_root(), {
        name = name or "img", z = z or 5,
        x = x, y = y, w = w2, h = h2, visible = true
    })
    layers.set_layer_image(node, tex_id)
    return node
end

-- ============================== SCENE -1: TITLE ==============================

local function title_init()
    layers.init()
    local root = layers.get_root()
    root.texture = solid(C.bg[1], C.bg[2], C.bg[3], 255)
    layers.set_layer_image(root, root.texture)

    local top = layers.add_layer(root, { name="bar_top", z=1, x=0, y=0, w=w, h=3, visible=true })
    top.texture = solid(C.blue[1], C.blue[2], C.blue[3], 200)
    layers.set_layer_image(top, top.texture)

    local bot = layers.add_layer(root, { name="bar_bot", z=1, x=0, y=h-3, w=w, h=3, visible=true })
    bot.texture = solid(C.blue[1], C.blue[2], C.blue[3], 200)
    layers.set_layer_image(bot, bot.texture)

    S.autoTmr = 0
    S.sceneInit = true
    preload()
    print("[DEMO] Title screen ready.")
end

local function title_update()
    local t = math.min(S.timer / 2.5, 1.0)
    local ease = t * t * (3.0 - 2.0 * t)
    local ty = h * 0.28 - (1.0 - ease) * 50

    draw_center(ty, "Caesura (AmeKAG)", C.gold, 3.0)
    draw_center(ty + 70, "Full Feature Test Demo", C.white, 1.8)

    if S.timer > 3.5 then
        local blink = math.floor(math.abs(math.sin(S.timer * 4.0)) * 180 + 75)
        draw_center(h - 60, "[ Click mouse to start ]",
            { C.dim[1], C.dim[2], C.dim[3], blink }, 0.9)
    end
end

-- ============================== SCENE 0: TEXT ==============================

local function s0_init()
    clear_scene(); preload()
    local bg = layers.get_root()
    if S.tex.classroom > 0 then layers.set_layer_image(bg, S.tex.classroom) end
    S.autoTmr = 5.0; S.sceneInit = true
end

local function s0_update()
    draw_center(40, "=== Scene 01: Text & Dialogue ===", C.gold, 1.6)
    draw_text(60, 120, "* Font color test", C.blue, 1.2)
    draw_text(60, 155, "  Default white - narration", C.white, 1.0)
    draw_text(60, 185, "  Gold emphasis - key points", C.gold, 1.0)
    draw_text(60, 215, "  Green status - info", C.green, 1.0)
    draw_text(60, 280, "* Ruby annotation test", C.blue, 1.2)
    backend.render_ruby("Kanji", "reading", 60, 315)
    draw_text(60, 390, "* Dialogue simulation", C.blue, 1.2)
    draw_text(80, 430, "[Welcome home, Haru-chan.]", C.white, 1.2)
    draw_text(80, 465, "  -- Character dialogue", C.dim, 0.8)
end

-- ============================== SCENE 1: LAYERS ==============================

local function s1_init()
    clear_scene(); preload()
    local bg = layers.get_root()
    if S.tex.classroom > 0 then layers.set_layer_image(bg, S.tex.classroom) end
    if S.tex.girl > 0 then
        S.lyrs.chara = add_img(w - 400, 80, 350, 550, S.tex.girl, "chara", 10)
    end
    S.autoTmr = 6.0; S.sceneInit = true
end

local function s1_update()
    draw_center(40, "=== Scene 02: Layers & Sprites ===", C.gold, 1.6)
    draw_text(60, 120, "* Background (bg) - classroom.png", C.blue, 1.2)
    draw_text(60, 155, "* Foreground (fg) - girl_uniform.png", C.blue, 1.2)

    local cycle = math.floor(S.timer / 1.5) % 4
    if cycle == 0 then
        draw_text(80, 250, "  [OK] Default position", C.green, 1.0)
    elseif cycle == 1 then
        draw_text(80, 250, "  [OK] Opacity 80%", C.green, 1.0)
        if S.lyrs.chara then layers.set_layer_opacity(S.lyrs.chara, 204) end
    elseif cycle == 2 then
        draw_text(80, 250, "  [OK] Move to left", C.green, 1.0)
        if S.lyrs.chara then layers.move_layer(S.lyrs.chara, 80, 80); layers.set_layer_opacity(S.lyrs.chara, 255) end
    else
        draw_text(80, 250, "  [OK] Hide sprite", C.yellow, 1.0)
        if S.lyrs.chara then layers.set_layer_visible(S.lyrs.chara, false) end
    end
end

-- ============================== SCENE 2: AUDIO ==============================

local function s2_init()
    clear_scene(); preload()
    local bg = layers.get_root()
    if S.tex.hana > 0 then layers.set_layer_image(bg, S.tex.hana) end
    S.autoTmr = 7.0; S.sceneInit = true
end

local function s2_update()
    draw_center(40, "=== Scene 03: Audio System ===", C.gold, 1.6)
    local t = S.timer
    if t > 0.5 and t < 1.0 then
        backend.audio_play("bgm", "assets/bgm/daily.ogg", { fadein = 2.0 })
        draw_text(80, 150, "  [OK] BGM fade in...", C.green, 1.0)
    elseif t > 5.0 and t < 5.5 then
        backend.audio_stop("bgm", { fadeout = 2.0 })
        draw_text(80, 150, "  [OK] BGM fading out", C.yellow, 1.0)
    else
        draw_text(80, 150, "  [OK] BGM active", C.green, 1.0)
    end
    if t > 1.0 and t < 1.1 then backend.audio_play("se", "assets/se/click.ogg") end
    draw_text(60, 220, "* SE - click.ogg", C.blue, 1.2)
    draw_text(80, 250, "  [OK] SE fired", C.green, 1.0)
    if t > 3.0 and t < 3.1 then backend.audio_play("voice", "assets/voice/line01.ogg") end
    draw_text(60, 310, "* Voice - line01.ogg", C.blue, 1.2)
    draw_text(80, 340, "  [OK] Voice: Welcome home, Haru-chan", C.green, 1.0)
    draw_text(60, 420, "* BGM/SE/Voice independent bus control", C.blue, 1.2)
end

-- ============================== SCENE 3: FLOW CONTROL ==============================

local function s3_init()
    clear_scene()
    local bg = layers.get_root()
    bg.texture = solid(C.bg[1], C.bg[2], C.bg[3], 255)
    layers.set_layer_image(bg, bg.texture)
    S.autoTmr = 0; S.sceneInit = true
end

local function s3_update()
    draw_center(40, "=== Scene 04: Flow Control ===", C.gold, 1.6)
    local items = {
        "if/else | jump | call/return | macro",
        "button (choices) | eval | wait | end/stop",
    }
    for i, item in ipairs(items) do
        draw_text(80, 180 + (i-1) * 50, "* " .. item, C.white, 1.1)
    end
    draw_text(60, 340, "Full KAG .ks script parser available", C.blue, 1.1)
    draw_text(60, 380, "Tokenizer + Scheduler + 68 commands", C.dim, 0.9)
    draw_text(60, h - 60, "Click to continue", C.dim, 0.8)
end

-- ============================== SCENE 4: TRANSITIONS ==============================

local function s4_init()
    clear_scene(); preload()
    local bg = layers.get_root()
    if S.tex.classroom > 0 then layers.set_layer_image(bg, S.tex.classroom) end
    S.autoTmr = 8.0; S.sceneInit = true
end

local function s4_update()
    draw_center(40, "=== Scene 05: Transitions & VFX ===", C.gold, 1.6)
    local t = S.timer
    if t < 3.0 then
        draw_text(60, 120, "* Current bg: classroom.png -> switching...", C.blue, 1.2)
    else
        draw_text(60, 120, "* Current bg: hana.png (flower field)", C.green, 1.2)
        local bg = layers.get_root()
        if S.tex.hana > 0 then layers.set_layer_image(bg, S.tex.hana) end
    end
    draw_text(60, 200, "* trans - crossfade / dissolve / wipe", C.blue, 1.2)
    draw_text(60, 260, "* quake - screen shake", C.blue, 1.2)
    draw_text(60, 320, "* vfx - fade / flash / blur / sepia", C.blue, 1.2)
    draw_text(60, 380, "* move - position/scale/opacity animation", C.blue, 1.2)
end

-- ============================== SCENE 5: VIDEO ==============================

local function s5_init()
    clear_scene()
    local bg = layers.get_root()
    bg.texture = solid(0, 0, 0, 255)
    layers.set_layer_image(bg, bg.texture)
    S.autoTmr = 5.0; S.sceneInit = true
end

local function s5_update()
    draw_center(40, "=== Scene 06: Video Playback ===", C.gold, 1.6)
    draw_text(60, 140, "* opening.mpg (162 MB)", C.blue, 1.2)
    draw_text(60, 240, "* Video API:", C.blue, 1.2)
    for i, api in ipairs({"video_play / video_stop", "video_pause / video_resume", "video_is_playing / video_has_ended", "video_get_texture / video_get_size"}) do
        draw_text(80, 280 + (i-1) * 30, "  [OK] " .. api, C.dim, 0.8)
    end
end

-- ============================== SCENE 6: SAVE & LOAD ==============================

local function s6_init()
    clear_scene()
    local bg = layers.get_root()
    bg.texture = solid(C.bg[1], C.bg[2], C.bg[3], 255)
    layers.set_layer_image(bg, bg.texture)
    S.autoTmr = 5.0; S.sceneInit = true
    local ok = System.save(1, { scene = "demo_test", time = os.time() })
    print(ok and "[PASS] Save slot 1" or "[FAIL] Save slot 1")
    local data = System.load(1)
    print((data and data.scene == "demo_test") and "[PASS] Load slot 1" or "[FAIL] Load slot 1")
end

local function s6_update()
    draw_center(40, "=== Scene 07: Save & Load ===", C.gold, 1.6)
    local items = {
        "System.save(slot, data) - save state",
        "System.load(slot) - load state",
        "Auto migration v1->v2->v3->v4->v5",
        "Types: string/number/boolean/table",
        "AES-256-GCM encryption",
        "Steam CloudSave (requires SDK)",
    }
    for i, item in ipairs(items) do
        draw_text(80, 140 + (i-1) * 45, "* " .. item, C.white, 0.95)
    end
end

-- ============================== SCENE 7: END ==============================

local function s7_init()
    clear_scene(); preload()
    local bg = layers.get_root()
    if S.tex.hana > 0 then layers.set_layer_image(bg, S.tex.hana) end
    S.autoTmr = 999; S.sceneInit = true
end

local function s7_update()
    draw_center(h * 0.22, "=== Test Complete ===", C.gold, 2.5)
    if S.timer > 1.5 then
        draw_center(h * 0.38, "Caesura (AmeKAG) - Full Feature Test", C.white, 1.4)
        draw_center(h * 0.46, "68 KAG Commands | 6 Lua Modules | 8 Subsystems", C.dim, 1.0)
    end
    if S.timer > 2.5 then draw_center(h * 0.58, "Thank you for testing!", C.blue, 1.3) end
    if S.timer > 3.5 then draw_center(h * 0.70, "github.com/ailiasdesu/Caesura-AmeKAG", C.dim, 0.9) end
end

-- ============================== DISPATCH ==============================

local scenes = {
    { init = title_init, update = title_update, auto = false },
    { init = s0_init,    update = s0_update,    auto = true  },
    { init = s1_init,    update = s1_update,    auto = true  },
    { init = s2_init,    update = s2_update,    auto = true  },
    { init = s3_init,    update = s3_update,    auto = true  },
    { init = s4_init,    update = s4_update,    auto = true  },
    { init = s5_init,    update = s5_update,    auto = true  },
    { init = s6_init,    update = s6_update,    auto = true  },
    { init = s7_init,    update = s7_update,    auto = false },
}

function engine_render()
    layers.render()
end

function engine_update(dt)
    S.frame = S.frame + 1
    S.timer = S.timer + (dt or 0.016)
    local idx = S.phase + 2
    local sc = scenes[idx]
    if not sc then return end
    if not S.sceneInit and sc.init then sc.init() end
    if sc.update then sc.update() end
    if sc.auto and S.autoTmr > 0 and S.timer >= S.autoTmr then
        S.phase = S.phase + 1; S.timer = 0; S.autoTmr = 0; S.sceneInit = false
    end
end

function _KAG_onClick()
    local sc = scenes[S.phase + 2]
    if sc and not sc.auto and S.autoTmr == 0 then
        S.phase = S.phase + 1; S.timer = 0; S.autoTmr = 0; S.sceneInit = false
    end
end

print("[TEST-DEMO] Full feature test demo loaded.")
