-- ===========================================================================
--  Caesura (AmeKAG) -- Demo Scene
--  4-phase visual demo, no external assets needed, download and run
-- ===========================================================================

local layers  = require("layers")
local backend = require("backend")

local w, h = backend.get_resolution()
if not w then w, h = 1280, 720 end

-- ============================== Scene State ==============================

local scene = {
    frame = 0,
    phase = 0,
    phaseTimer = 0,
    bg = nil,
    logoRect = nil,
}

-- ============================== Color Palette ==============================

local C = {
    bgDark    = { 8,  12, 28, 255 },
    bgMid     = { 18, 24, 48, 255 },
    accent    = { 80, 160, 255, 255 },
    accent2   = { 220, 120, 60, 255 },
    white     = { 240, 240, 250, 255 },
    dimWhite  = { 160, 170, 200, 255 },
    green     = { 60, 200, 100, 255 },
    purple    = { 140, 80, 220, 255 },
    card      = { 18, 22, 42, 220 },
}

local function solid(r, g, b, a)
    return backend.create_solid_texture(
        math.floor(r), math.floor(g), math.floor(b), math.floor(a or 255))
end

-- ============================== Text Rendering ==============================

local function draw_text(x, y, text, color, scale)
    backend.render_text(text, x, y, scale or 1.0,
        color[1], color[2], color[3], color[4])
end

local function draw_centered(y, text, color, scale)
    local tw = #text * (scale or 1.0) * 10
    local x = (w - tw) / 2
    draw_text(x, y, text, color, scale)
end

-- ============================== Scene Init ==============================

function scene_init()
    layers.init()
    scene.bg = layers.get_root()
    scene.bg.name = "bg"

    -- Background fill
    scene.bg.texture = solid(C.bgDark[1], C.bgDark[2], C.bgDark[3], 255)
    layers.set_layer_image(scene.bg, scene.bg.texture)

    -- Decorative elements
    local topLine = layers.add_layer(scene.bg, {
        name = "top_line", z = 1, x = 0, y = 0, w = w, h = 2, visible = true
    })
    topLine.texture = solid(C.accent[1], C.accent[2], C.accent[3], 255)
    layers.set_layer_image(topLine, topLine.texture)

    local bottomLine = layers.add_layer(scene.bg, {
        name = "bottom_line", z = 1, x = 0, y = h - 2, w = w, h = 2, visible = true
    })
    bottomLine.texture = solid(C.accent[1], C.accent[2], C.accent[3], 255)
    layers.set_layer_image(bottomLine, bottomLine.texture)
end

-- ============================== Per-Frame Update ==============================

function engine_render()
    layers.render()
end

function engine_update(dt)
    if not scene.bg then scene_init() end
    scene.frame = scene.frame + 1
    scene.phaseTimer = scene.phaseTimer + (dt or 0.016)

    if scene.phase == 0 then update_title()
    elseif scene.phase == 1 then update_features()
    elseif scene.phase == 2 then update_tech()
    elseif scene.phase == 3 then update_end()
    end
end

-- ================================================================
--  Phase 0: Title Screen (0 -> 5s)
-- ================================================================

function update_title()
    local t = math.min(scene.phaseTimer / 2.0, 1.0)
    local ease = t * t * (3.0 - 2.0 * t)
    local ty = h * 0.35 - (1.0 - ease) * 40

    draw_centered(ty, "Caesura (AmeKAG)", C.white, 2.5)

    local alpha = math.floor(200 * math.min(scene.phaseTimer / 3.0, 1.0))
    draw_centered(ty + 80, "Next-Gen Visual Novel Engine", C.accent, 1.6)

    local blink = math.floor(math.abs(math.sin(scene.phaseTimer * 3.0)) * 200 + 55)
    if scene.phaseTimer > 3.0 then
        draw_centered(h - 80, "[ Click or press any key to continue ]", C.dimWhite, 0.9)
    end

    -- Background breathing pulse (every ~2 seconds via opacity)
    if scene.frame % 120 == 0 and scene.bg then
        local pulse = 1.0 + math.sin(scene.frame * 0.02) * 0.08
        scene.bg.opacity = math.floor(200 + 55 * pulse)
        layers.mark_dirty(scene.bg)
    end

    _next_phase(5.0, 1)
end

-- ================================================================
--  Phase 1: Feature Showcase (5 -> 13.5s)
-- ================================================================

local features = {
    { "KAG Script Compatible",    "53+ command handlers" },
    { "Multiplatform Rendering",  "bgfx: D3D11 / Metal / OpenGL" },
    { "Live2D Cubism 5",          "Native SDK integration" },
    { "SoLoud Audio Engine",      "BGM / Voice / SFX buses" },
    { "Lua Scripting",            "Full KAG parser + runtime" },
    { "Modern C++20 Core",        "Job system, async loader, zstd" },
}

function update_features()
    local t = math.min(scene.phaseTimer / 1.5, 1.0)

    for i, feat in ipairs(features) do
        local y = h * 0.12 + (i - 1) * 55
        local rowT = math.min(math.max((t * 8.0 - (i - 1) * 0.6), 0), 1.0)
        local alpha = math.floor(rowT * 255)
        draw_text(w * 0.15, y, feat[1], { C.accent[1], C.accent[2], C.accent[3], alpha }, 1.1)
        draw_text(w * 0.15, y + 24, feat[2], C.dimWhite, 0.8 * rowT)
    end

    -- Bottom progress bar
    local barW = w * 0.6
    local progress = math.min((scene.phaseTimer - 0.5) / (13.5 - 0.5), 1.0)
    local barBg = layers.find(function(n) return n.name == "progress_bg" end)
    if not barBg then
        barBg = layers.add_layer(layers.get_root(), {
            name = "progress_bg", z = 3, x = w * 0.2, y = h - 140, w = barW, h = 4, visible = true
        })
        barBg.texture = solid(C.bgMid[1], C.bgMid[2], C.bgMid[3], 60)
        layers.set_layer_image(barBg, barBg.texture)
    end
    local barFg = layers.find(function(n) return n.name == "progress_fg" end)
    if not barFg then
        barFg = layers.add_layer(layers.get_root(), {
            name = "progress_fg", z = 4, x = w * 0.2, y = h - 140, w = 1, h = 4, visible = true
        })
        barFg.texture = solid(C.accent[1], C.accent[2], C.accent[3], 255)
        layers.set_layer_image(barFg, barFg.texture)
    end
    barFg.w = barW * progress
    if scene.frame % 30 == 0 then
        barFg.w = 200 * progress
        layers.mark_dirty(barFg)
    end

    _next_phase(13.5, 2)
end

-- ================================================================
--  Phase 2: Tech Stack (13.5 -> 21.5s)
-- ================================================================

local techItems = {
    { "SDL3",      "Platform layer",     C.accent },
    { "bgfx",      "Cross-platform GPU", C.accent2 },
    { "SoLoud",    "Audio engine",       C.green },
    { "Lua 5.4",   "Scripting runtime",  C.purple },
    { "FreeType",  "Font rendering",     C.accent },
    { "zstd",      "Asset compression",  C.accent2 },
}

function update_tech()
    local t = math.min(scene.phaseTimer / 0.8, 1.0)

    for i, item in ipairs(techItems) do
        local col = (i - 1) % 3
        local row = math.floor((i - 1) / 3)
        local cx = w * 0.17 + col * w * 0.22 + w * 0.11
        local cy = h * 0.25 + row * 120
        local itemT = math.min(math.max((t * 7.0 - (i - 1) * 0.5), 0), 1.0)
        local ease = itemT * itemT * (3.0 - 2.0 * itemT)
        local alpha = math.floor(ease * 255)

        local cardBg = layers.find(function(n) return n.name == "card_" .. i end)
        if not cardBg then
            cardBg = layers.add_layer(layers.get_root(), {
                name = "card_" .. i, z = 2,
                x = cx - 90, y = cy - 35, w = 180, h = 70, visible = true
            })
            cardBg.texture = solid(C.card[1], C.card[2], C.card[3], C.card[4])
            layers.set_layer_image(cardBg, cardBg.texture)
        end
        cardBg.visible = itemT > 0.1
        draw_text(cx - 40, cy - 10, item[1], { item[3][1], item[3][2], item[3][3], alpha }, 1.2)
        draw_text(cx - 40, cy + 14, item[2], { C.dimWhite[1], C.dimWhite[2], C.dimWhite[3], alpha }, 0.8)
    end

    _next_phase(21.5, 3)
end

-- ================================================================
--  Phase 3: End Screen (21.5 -> inf)
-- ================================================================

function update_end()
    local t = math.min(scene.phaseTimer / 2.0, 1.0)
    local ease = t * t * (3.0 - 2.0 * t)

    draw_centered(h * 0.3, "Thank you for watching!", C.white, 2.2 * ease)

    if scene.phaseTimer > 3.0 then
        local alpha = math.floor(240 * ease)
        draw_centered(h * 0.5, "github.com/ailiasdesu/Caesura-AmeKAG", C.accent, 1.1)
        draw_centered(h * 0.6, "Built with SDL3 + bgfx + SoLoud + Lua", C.dimWhite, 0.9)
        draw_centered(h * 0.7, "Cross-platform | Open Source | Visual Novel Engine", C.dimWhite, 0.8)
    end
end

-- ============================== Helpers ==============================

function _next_phase(delay, nextPhase)
    if scene.phaseTimer >= delay then
        scene.phase = nextPhase
        scene.phaseTimer = 0
    end
end
