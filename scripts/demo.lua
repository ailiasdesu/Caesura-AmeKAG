-- ===========================================================================
--  Caesura (AmeKAG) — Demo Scene
--  下载即看的视觉演示，无需外部素材
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
    subtitle = "",
    subtitleAlpha = 0,
    bg = nil,
    logoRect = nil,
    textBlocks = {},
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
    return backend.create_solid_texture(r, g, b, a or 255)
end

-- ============================== 文字渲染辅助 ==============================

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

    -- 背景层
    scene.bg = layers.add_layer(layers.get_root(), {
        name = "demo_bg", z = 0, x = 0, y = 0, w = w, h = h, visible = true
    })
    scene.bg.texture = solid(C.bgDark[1], C.bgDark[2], C.bgDark[3], C.bgDark[4])
    layers.set_layer_image(scene.bg, scene.bg.texture)

    -- Logo 装饰矩形
    scene.logoRect = layers.add_layer(layers.get_root(), {
        name = "logo_deco", z = 2, x = w/2 - 180, y = 100, w = 360, h = 4,
        visible = true
    })
    scene.logoRect.texture = solid(C.accent[1], C.accent[2], C.accent[3], C.accent[4])
    layers.set_layer_image(scene.logoRect, scene.logoRect.texture)

    -- 底部装饰线
    local bottomLine = layers.add_layer(layers.get_root(), {
        name = "bottom_line", z = 1, x = w/2 - 300, y = h - 120,
        w = 600, h = 2, visible = true
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

    if scene.phase == 0 then
        update_title()
    elseif scene.phase == 1 then
        update_features()
    elseif scene.phase == 2 then
        update_tech()
    elseif scene.phase == 3 then
        update_end()
    end
end

-- ============================== 阶段0: 标题画面 ==============================

function update_title()
    -- 渐入
    local t = math.min(scene.phaseTimer / 2.0, 1.0)
    local easeIn = t * t * (3 - 2 * t)  -- smoothstep

    -- 标题
    draw_centered(200, "Caesura", C.white, 3.0 * easeIn)
    draw_centered(260, "(AmeKAG)", C.dimWhite, 1.5 * easeIn)

    -- 装饰线跟随渐入
    if scene.logoRect then
        scene.logoRect.w = 360 * easeIn
    end

    -- 副标题
    if t > 0.6 then
        local st = (t - 0.6) / 0.4
        draw_centered(350, "Cross-platform Visual Novel Engine", C.accent, 1.0 * st)
    end

    -- 版本号
    if t > 0.8 then
        local vt = (t - 0.8) / 0.2
        draw_centered(400, "v1.0.0-alpha", C.dimWhite, 0.8 * vt)
    end

    -- 提示
    if t > 0.9 and math.floor(scene.frame / 60) % 2 == 0 then
        draw_centered(h - 80, "[ Click or press any key to continue ]", C.dimWhite, 0.9)
    end

    -- 背景色微调
    if scene.bg and scene.bg.texture then
        local pulse = 1.0 + math.sin(scene.frame * 0.02) * 0.08
        backend.destroy_texture(scene.bg.texture)
        scene.bg.texture = solid(
            C.bgDark[1] * pulse, C.bgDark[2] * pulse, C.bgDark[3] * pulse, 255)
        layers.set_layer_image(scene.bg, scene.bg.texture)
    end

    _next_phase(5.0, 1)
end

-- ============================== 阶段1: 功能展示 ==============================

local features = {
    { "KAG Script Compatible",    "Full KAG tag parser, 53 command handlers" },
    { "Lua Sandbox Security",     "Track 3 strict mode, AI-auditable rules" },
    { "CARC Encrypted Packaging", "ed25519 signature + zstd compression" },
    { "Particle Effects (VFX)",   "Quake, flash, blur, fade, snow, rain" },
    { "3D LUT Color Grading",     "Real-time palette-based color correction" },
}

function update_features()
    local t = math.min(scene.phaseTimer / 0.5, 1.0)
    local idx = math.min(math.floor(scene.phaseTimer / 2.5) + 1, #features)

    -- 标题
    draw_centered(60, "Features", C.accent, 2.0)

    local yBase = 180
    for i = 1, #features do
        local alpha = 1.0
        local color = C.dimWhite
        if i == idx then
            color = C.white
            alpha = 1.0
        elseif i < idx then
            color = C.green
            alpha = 0.7
        end
        draw_text(180, yBase + (i-1) * 70, features[i][1], color, 1.3 * alpha)
        if i <= idx then
            draw_text(200, yBase + (i-1) * 70 + 28, features[i][2], C.dimWhite, 0.75 * alpha)
        end
    end

    -- 进度条
    local progress = idx / #features
    local barW = 400
    local barX = w/2 - barW/2
    local barY = h - 150
    -- 背景
    local barBg = layers.find(function(n) return n.name == "progress_bg" end)
    if not barBg then
        barBg = layers.add_layer(layers.get_root(), {
            name = "progress_bg", z = 3, x = barX, y = barY, w = barW, h = 4, visible = true
        })
        barBg.texture = solid(C.dimWhite[1], C.dimWhite[2], C.dimWhite[3], 60)
        layers.set_layer_image(barBg, barBg.texture)
    end
    local barFg = layers.find(function(n) return n.name == "progress_fg" end)
    if not barFg then
        barFg = layers.add_layer(layers.get_root(), {
            name = "progress_fg", z = 4, x = barX, y = barY, w = 1, h = 4, visible = true
        })
    end
    if barFg.texture then backend.destroy_texture(barFg.texture) end
    barFg.texture = solid(C.accent[1], C.accent[2], C.accent[3], 255)
    barFg.w = barW * progress
    layers.set_layer_image(barFg, barFg.texture)

    _next_phase(13.5, 2)
end

-- ============================== 阶段2: 技术栈 ==============================

local techItems = {
    { "SDL3",      "Platform abstraction layer",     C.accent },
    { "bgfx",      "Cross-platform GPU renderer",    C.accent2 },
    { "SoLoud",    "Audio engine (3 buses)",         C.green },
    { "Lua 5.4",   "Embedded scripting runtime",     C.purple },
    { "FreeType",  "TrueType font rasterizer",       C.white },
    { "zstd",      "Fast compression",               C.dimWhite },
}

function update_tech()
    local t = math.min(scene.phaseTimer / 0.5, 1.0)

    draw_centered(60, "Tech Stack", C.accent, 2.0)

    local cols = 3
    local cardW = 200
    local cardH = 90
    local gapX = 40
    local gapY = 24
    local totalW = cols * cardW + (cols - 1) * gapX
    local startX = (w - totalW) / 2
    local startY = 160

    for i = 1, #techItems do
        local col = (i - 1) % cols
        local row = math.floor((i - 1) / cols)
        local cx = startX + col * (cardW + gapX)
        local cy = startY + row * (cardH + gapY)

        -- 卡片延迟出现
        local delay = (i - 1) * 0.3
        local cardT = math.min(math.max((scene.phaseTimer - delay) / 0.4, 0), 1.0)
        local alpha = 220 * cardT

        -- 卡片背景
        local cardName = "tech_card_" .. i
        local card = layers.find(function(n) return n.name == cardName end)
        if not card and cardT > 0.01 then
            card = layers.add_layer(layers.get_root(), {
                name = cardName, z = 5 + i, x = cx, y = cy,
                w = cardW, h = cardH, visible = true
            })
            card.texture = solid(C.card[1], C.card[2], C.card[3], C.card[4])
            layers.set_layer_image(card, card.texture)
        end
        if card and card.texture and cardT > 0.01 then
            backend.destroy_texture(card.texture)
            card.texture = solid(C.card[1], C.card[2], C.card[3], alpha)
            layers.set_layer_image(card, card.texture)
        end

        -- 文字
        if cardT > 0.5 then
            local tc = techItems[i][3]
            draw_text(cx + 12, cy + 14, techItems[i][1], tc, 1.4)
            draw_text(cx + 12, cy + 48, techItems[i][2], C.dimWhite, 0.7)
        end
    end

    _next_phase(8.0, 3)
end

-- ============================== 阶段3: 结束画面 ==============================

function update_end()
    local t = math.min(scene.phaseTimer / 1.5, 1.0)
    local easeIn = t * t * (3 - 2 * t)

    draw_centered(h/2 - 40, "Ready for Your Story", C.white, 2.5 * easeIn)
    draw_centered(h/2 + 20, "Drop a .ks script, build, and run.", C.dimWhite, 1.0 * easeIn)

    if t > 0.7 then
        local st = (t - 0.7) / 0.3
        draw_centered(h/2 + 80, "github.com/ailiasdesu/Caesura-AmeKAG", C.accent, 0.8 * st)
    end

    if t > 0.95 and math.floor(scene.frame / 60) % 2 == 0 then
        draw_centered(h - 60, "[ Press ESC or close window to exit ]", C.dimWhite, 0.8)
    end
end

-- ============================== 辅助函数 ==============================

function _next_phase(delay, nextPhase)
    if scene.phaseTimer >= delay then
        -- 清理上一阶段的动态层
        if nextPhase == 2 then
            local barBg = layers.find(function(n) return n.name == "progress_bg" end)
            if barBg then layers.remove_layer(barBg) end
            local barFg = layers.find(function(n) return n.name == "progress_fg" end)
            if barFg then layers.remove_layer(barFg) end
        elseif nextPhase == 3 then
            for i = 1, #techItems do
                local card = layers.find(function(n) return n.name == "tech_card_" .. i end)
                if card then layers.remove_layer(card) end
            end
        end
        scene.phase = nextPhase
        scene.phaseTimer = 0
    end
end

-- ============================== 输入处理 ==============================

function _KAG_onClick(x, y)
    -- 快进到下一阶段
    if scene.phase < 3 then
        scene.phaseTimer = 999  -- 触发 _next_phase
    end
end

function _KAG_onKey(key, mods)
    _KAG_onClick(0, 0)
end

-- ============================== 启动 ==============================

scene_init()
print("[DEMO] Caesura (AmeKAG) demo scene loaded.")
print("[DEMO] Click or press any key to advance.")
