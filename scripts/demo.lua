-- ===========================================================================
--  Caesura (AmeKAG) — Demo Scene
--  4 阶段视觉演示，无需外部素材，下载即看
-- ===========================================================================

local layers  = require("layers")
local backend = require("backend")

local w, h = backend.get_resolution()
if not w then w, h = 1280, 720 end

-- ============================== 场景状态 ==============================

local scene = {
    frame = 0,
    phase = 0,          -- 0=title, 1=features, 2=tech, 3=end
    phaseTimer = 0,
    bg = nil,
    logoRect = nil,
}

-- ============================== 颜色调色板 ==============================

local C = {
    bgDark    = { 8,  12, 28, 255 },   -- 深蓝黑
    bgMid     = { 18, 24, 48, 255 },   -- 中蓝
    accent    = { 80, 160, 255, 255 }, -- 亮蓝
    accent2   = { 220, 120, 60, 255 }, -- 暖橙
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

-- ============================== 文字渲染 ==============================

local function draw_text(x, y, text, color, scale)
    backend.render_text(text, x, y, scale or 1.0,
        color[1]/255, color[2]/255, color[3]/255, color[4]/255)
end

local function draw_centered(y, text, color, scale)
    local approxW = #text * (scale or 1.0) * 8
    local x = (w - approxW) / 2
    draw_text(x, y, text, color, scale)
end

-- ============================== 场景构建 ==============================

function scene_init()
    layers.init()

    scene.bg = layers.add_layer(layers.get_root(), {
        name = "demo_bg", z = 0, x = 0, y = 0, w = w, h = h, visible = true
    })
    scene.bg.texture = solid(C.bgDark[1], C.bgDark[2], C.bgDark[3], C.bgDark[4])
    layers.set_layer_image(scene.bg, scene.bg.texture)

    scene.logoRect = layers.add_layer(layers.get_root(), {
        name = "logo_deco", z = 2, x = w/2 - 180, y = 100, w = 360, h = 4, visible = true
    })
    scene.logoRect.texture = solid(C.accent[1], C.accent[2], C.accent[3], C.accent[4])
    layers.set_layer_image(scene.logoRect, scene.logoRect.texture)

    local bottomLine = layers.add_layer(layers.get_root(), {
        name = "bottom_line", z = 1, x = w/2 - 300, y = h - 120, w = 600, h = 2, visible = true
    })
    bottomLine.texture = solid(C.accent[1], C.accent[2], C.accent[3], 100)
    layers.set_layer_image(bottomLine, bottomLine.texture)
end

-- ============================== 每帧更新 ==============================

function engine_render()
    layers.render()
end

function engine_update(dt)
    scene.frame = scene.frame + 1
    scene.phaseTimer = scene.phaseTimer + dt
    if scene.phase == 0 then update_title()
    elseif scene.phase == 1 then update_features()
    elseif scene.phase == 2 then update_tech()
    elseif scene.phase == 3 then update_end()
    end
end

-- ================================================================
--  阶段0: 标题画面（0→5s）
--  元素：引擎名、版本号、装饰线、点击提示
-- ================================================================

function update_title()
    local t = math.min(scene.phaseTimer / 2.0, 1.0)
    local easeIn = t * t * (3 - 2 * t)

    draw_centered(200, "Caesura", C.white, 3.0 * easeIn)
    draw_centered(260, "(AmeKAG)", C.dimWhite, 1.5 * easeIn)

    if scene.logoRect then scene.logoRect.w = 360 * easeIn end

    if t > 0.6 then
        draw_centered(350, "Cross-platform Visual Novel Engine", C.accent, 1.0 * ((t - 0.6) / 0.4))
    end
    if t > 0.8 then
        draw_centered(400, "v1.0.0-alpha", C.dimWhite, 0.8 * ((t - 0.8) / 0.2))
    end
    if t > 0.9 and math.floor(scene.frame / 60) % 2 == 0 then
        draw_centered(h - 80, "[ Click or press any key to continue ]", C.dimWhite, 0.9)
    end

    -- 背景呼吸脉冲（每120帧一次，约2秒）
    if scene.frame % 120 == 0 and scene.bg then
        local pulse = 1.0 + math.sin(scene.frame * 0.02) * 0.08
        if scene.bg.texture then backend.destroy_texture(scene.bg.texture) end
        scene.bg.texture = solid(C.bgDark[1] * pulse, C.bgDark[2] * pulse, C.bgDark[3] * pulse, 255)
        layers.set_layer_image(scene.bg, scene.bg.texture)
    end

    _next_phase(5.0, 1)
end

-- ================================================================
--  阶段1: 功能展示（5→13.5s）
--  元素：6 个功能条目 + 进度条
-- ================================================================

local features = {
    { "KAG Script Compatible",    "53+ command handlers" },
    { "Lua Sandbox Security",     "Track 3 strict mode, AI-auditable" },
    { "D3D11 / bgfx Renderer",    "RTT-based layer compositing" },
    { "Hot Reload",               "Save scripts, see changes instantly" },
    { "Multi-bus Audio",          "BGM + Voice + SE, xfade, 3D spatial" },
    { "CARC Packaging",           "ed25519 signature + zstd compression" },
}

function update_features()
    draw_centered(60, "Feature Highlights", C.accent, 2.0)

    local startY = 160
    for i = 1, #features do
        local rowT = math.min(math.max((scene.phaseTimer - (i-1)*0.3) / 0.4, 0), 1.0)
        local y = startY + (i-1) * 48
        draw_text(w * 0.15, y, features[i][1], C.accent2, 1.4 * rowT)
        draw_text(w * 0.15, y + 24, features[i][2], C.dimWhite, 0.8 * rowT)
    end

    -- 底部进度条
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
    end
    barFg.w = barW * progress
    if scene.frame % 30 == 0 then
        if barFg.texture then backend.destroy_texture(barFg.texture) end
        barFg.texture = solid(C.accent[1], C.accent[2], C.accent[3], 255)
        layers.set_layer_image(barFg, barFg.texture)
    end

    _next_phase(13.5, 2)
end

-- ================================================================
--  阶段2: 技术栈（13.5→21.5s）
--  元素：3×2 卡片矩阵（SDL3/bgfx/SoLoud/Lua/FreeType/zstd）
-- ================================================================

local techItems = {
    { "SDL3",      "Platform layer",     C.accent },
    { "bgfx",      "GPU renderer",       C.accent2 },
    { "SoLoud",    "Audio engine",       C.green },
    { "Lua 5.4",   "Scripting runtime",  C.purple },
    { "FreeType",  "Font rasterizer",    C.white },
    { "zstd",      "Compression",        C.dimWhite },
}

function update_tech()
    draw_centered(60, "Tech Stack", C.accent, 2.0)

    local cols, cardW, cardH, gapX, gapY = 3, 200, 90, 40, 24
    local startX = (w - (cols*cardW + (cols-1)*gapX)) / 2
    local startY = 160

    for i = 1, #techItems do
        local cx = startX + ((i-1) % cols) * (cardW + gapX)
        local cy = startY + math.floor((i-1) / cols) * (cardH + gapY)
        local cardT = math.min(math.max((scene.phaseTimer - (i-1)*0.3) / 0.4, 0), 1.0)

        local cardName = "tech_card_" .. i
        local card = layers.find(function(n) return n.name == cardName end)
        if not card and cardT > 0.01 then
            card = layers.add_layer(layers.get_root(), {
                name = cardName, z = 5 + i, x = cx, y = cy, w = cardW, h = cardH, visible = true
            })
            card.texture = solid(C.card[1], C.card[2], C.card[3], 220)
            layers.set_layer_image(card, card.texture)
        end
        if cardT > 0.5 then
            draw_text(cx + 12, cy + 14, techItems[i][1], techItems[i][3], 1.4)
            draw_text(cx + 12, cy + 48, techItems[i][2], C.dimWhite, 0.7)
        end
    end

    _next_phase(8.0, 3)
end

-- ================================================================
--  阶段3: 结束画面（21.5→∞）
--  元素：号召性用语 + GitHub 链接
-- ================================================================

function update_end()
    local t = math.min(scene.phaseTimer / 1.5, 1.0)
    local easeIn = t * t * (3 - 2 * t)

    draw_centered(h/2 - 40, "Ready for Your Story", C.white, 2.5 * easeIn)
    draw_centered(h/2 + 20, "Drop a .ks script, build, and run.", C.dimWhite, 1.0 * easeIn)

    if t > 0.7 then
        draw_centered(h/2 + 80, "github.com/ailiasdesu/Caesura-AmeKAG", C.accent, 0.8 * ((t-0.7)/0.3))
    end
    if t > 0.95 and math.floor(scene.frame / 60) % 2 == 0 then
        draw_centered(h - 60, "[ Press ESC or close window to exit ]", C.dimWhite, 0.8)
    end
end

-- ============================== 辅助函数 ==============================

function _next_phase(delay, nextPhase)
    if scene.phaseTimer >= delay then
        scene.phase = nextPhase
        scene.phaseTimer = 0
    end
end

function _KAG_onClick(x, y)
    if scene.phase < 3 then scene.phaseTimer = 999 end
end

function _KAG_onKey(key, mods)
    _KAG_onClick(0, 0)
end

scene_init()
print("[DEMO] Caesura (AmeKAG) demo scene loaded.")
