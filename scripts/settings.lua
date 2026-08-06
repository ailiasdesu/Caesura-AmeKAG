-- ===========================================================================
--  Caesura (AmeKAG) — settings.lua  [P1.4]
--  Interactive settings menu using layer-based UI.
--  Volume sliders, text speed, skip/auto toggles, fullscreen, language.
--  Input focus switches to GAME mode during settings to prevent KAG misinput.
-- ===========================================================================

local Settings = {}
local backend = require("backend")
local layers  = require("layers")
local audio   = require("audio")
local i18n    = require("i18n")

-- ===========================================================================
-- Settings state
-- ===========================================================================
local state = {
    active = false,
    cursor = 1,         -- selected menu item index
    items = {},         -- { {label, key, type, value, min, max, step, action}, ... }
    bgLayer = nil,
    cursorLayer = nil,
    panelLayer = nil,   -- centered panel background
    borderLayer = nil,  -- panel border
}

-- Helper: safe solid texture
local function solid(r, g, b, a)
    return backend.create_solid_texture(math.floor(r), math.floor(g), math.floor(b), math.floor(a or 255))
end

-- ===========================================================================
-- Settings defaults
-- ===========================================================================
local defaults = {
    volume_bgm   = 80,
    volume_se    = 80,
    volume_voice = 100,
    text_speed   = 50,   -- ms per char
    fullscreen   = false,
    skip_mode    = false,
    skip_auto    = false,  -- read-skip: force-past unseen text at 2x instead of stopping
    auto_mode    = false,
    autosave_interval = 60,   -- seconds; 0 disables the auto-save timer
}

-- ===========================================================================
-- Settings._buildMenu(ctx) -> items table
-- ===========================================================================
function Settings._buildMenu(ctx)
    ctx.settingsValues = ctx.settingsValues or {}
    local sv = ctx.settingsValues
    for k, v in pairs(defaults) do
        if sv[k] == nil then sv[k] = v end
    end

    local items = {
        {label = i18n.t("volume_bgm"),   key = "volume_bgm",   type = "slider", value = sv.volume_bgm,   min = 0, max = 100},
        {label = i18n.t("volume_se"),    key = "volume_se",    type = "slider", value = sv.volume_se,    min = 0, max = 100},
        {label = i18n.t("volume_voice"), key = "volume_voice", type = "slider", value = sv.volume_voice, min = 0, max = 100},
        {label = i18n.t("text_speed"),   key = "text_speed",   type = "slider", value = sv.text_speed,   min = 10, max = 200},
        {label = i18n.t("skip_mode"),    key = "skip_mode",    type = "toggle", value = sv.skip_mode},
        {label = i18n.t("skip_auto"),    key = "skip_auto",    type = "toggle", value = sv.skip_auto},
        {label = i18n.t("auto_mode"),    key = "auto_mode",    type = "toggle", value = sv.auto_mode},
        {label = i18n.t("fullscreen"),   key = "fullscreen",   type = "toggle", value = sv.fullscreen},
        {label = i18n.t("language") .. ": " .. (i18n.current or "zh"),
         key = "language", type = "cycle", value = i18n.current,
         options = i18n.available()},
        {label = i18n.t("autosave_interval") .. ": " ..
             (sv.autosave_interval > 0 and sv.autosave_interval .. "s" or "Off"),
         key = "autosave_interval", type = "cycle", value = sv.autosave_interval,
         options = {30, 60, 120, 0}},
        {label = i18n.t("back"), key = "back", type = "action"},
    }
    return items
end

-- Settings._adjust(ctx, dir) — slider +/- / toggle flip / cycle next
function Settings._adjust(ctx, dir)
    local item = state.items and state.items[state.cursor]
    if not item then return end
    local sv = ctx.settingsValues or {}
    if item.type == "slider" then
        local v = (sv[item.key] or item.value or 0) + dir * 5
        sv[item.key] = math.max(item.min, math.min(item.max, v))
    elseif item.type == "toggle" then
        sv[item.key] = not (sv[item.key] == true)
    elseif item.type == "cycle" then
        local opts = item.options or {}
        local idx = 0
        for i, o in ipairs(opts) do if o == sv[item.key] then idx = i break end end
        idx = ((idx + dir - 1) % #opts) + 1
        sv[item.key] = opts[idx]
        if item.key == "language" and i18n and i18n.load then
            pcall(function() i18n.load(opts[idx]) end)
        end
    end
end

-- ===========================================================================
-- Settings.show(ctx) — open settings menu
-- ===========================================================================
function Settings.show(ctx)
    if state.active then return end
    state.active = true
    ctx._settingsActive = true
    state.items = Settings._buildMenu(ctx)
    state.cursor = 1

    -- Switch input focus to GAME to prevent KAG text advancement
    backend.set_input_focus("GAME")

    local w, h = backend.get_resolution()
    w = w or 1280
    h = h or 720

    -- Panel dimensions (centered, with border)
    local panelW = 680
    local panelH = 420
    local panelX = (w - panelW) / 2
    local panelY = (h - panelH) / 2

    -- Layer 1: Dark semi-transparent full-screen background
    local bg = layers.ensure(ctx, "_settings_bg", 94)
    bg.visible = true
    bg.x, bg.y = 0, 0
    bg.w, bg.h = w, h
    bg.texture = solid(0, 0, 0, 160)
    state.bgLayer = bg

    -- Layer 2: Panel border (2px visible border around the panel)
    local border = layers.ensure(ctx, "_settings_border", 95)
    border.visible = true
    border.x, border.y = panelX - 2, panelY - 2
    border.w, border.h = panelW + 4, panelH + 4
    border.texture = solid(80, 80, 180, 255)
    state.borderLayer = border

    -- Layer 3: Panel background (slightly dark blue-ish panel)
    local panel = layers.ensure(ctx, "_settings_panel", 96)
    panel.visible = true
    panel.x, panel.y = panelX, panelY
    panel.w, panel.h = panelW, panelH
    panel.texture = solid(15, 15, 40, 235)
    state.panelLayer = panel

    -- Layer 4: Cursor highlight layer (colored rectangle behind selected row)
    local cursor = layers.ensure(ctx, "_settings_cursor", 97)
    cursor.visible = true
    cursor.texture = solid(60, 100, 200, 100)
    cursor.w = panelW - 80
    cursor.h = 28
    state.cursorLayer = cursor

    -- Store panel geometry for rendering
    state.panelX = panelX
    state.panelY = panelY
    state.panelW = panelW
    state.panelH = panelH

    -- INPUT LOOP (audit: show() rendered once and returned -- the
    -- 'Up/Down to navigate' hint was a lie; the settings menu was
    -- dead). Same per-frame pattern as Gallery/MusicRoom.
    while true do
        Settings._render(ctx)
        coroutine.yield()

        if _G._GAME_KEY_UP == true then
            _G._GAME_KEY_UP = false
            state.cursor = math.max(1, state.cursor - 1)
        elseif _G._GAME_KEY_DOWN == true then
            _G._GAME_KEY_DOWN = false
            state.cursor = math.min(#state.items, state.cursor + 1)
        elseif _G._GAME_KEY_LEFT == true then
            _G._GAME_KEY_LEFT = false
            Settings._adjust(ctx, -1)
        elseif _G._GAME_KEY_RIGHT == true then
            _G._GAME_KEY_RIGHT = false
            Settings._adjust(ctx, 1)
        elseif _G._GAME_KEY_ENTER == true then
            _G._GAME_KEY_ENTER = false
            local item = state.items[state.cursor]
            if item and item.type == "action" then
                if item.key == "back" then break end
            else
                Settings._adjust(ctx, 1)
            end
        elseif _G._GAME_KEY_ESC == true then
            _G._GAME_KEY_ESC = false
            break
        end
    end
    Settings.hide(ctx)
end

-- ===========================================================================
-- Settings.hide(ctx) — close settings, restore KAG focus
-- ===========================================================================
function Settings.hide(ctx)
    state.active = false
    ctx._settingsActive = false
    for _, name in ipairs({"_settings_bg", "_settings_border", "_settings_panel", "_settings_cursor"}) do
        local layer = layers.find(name)
        if layer then
            layer.visible = false
            if layer.texture then backend.destroy_texture(layer.texture); layer.texture = nil end
        end
    end
    -- Save settings values
    if ctx.settingsValues then
        Settings._applyAll(ctx)
    end
    backend.set_input_focus("KAG")
    state.items = {}
    state.panelX = nil; state.panelY = nil; state.panelW = nil; state.panelH = nil
    print("[Settings] Menu closed.")
end

-- ===========================================================================
-- Settings._renderVolumeBar(value, max) -> string
-- ===========================================================================
function Settings._renderVolumeBar(value, max)
    local blocks = math.floor(value / (max / 10))  -- 10 segments
    local filled = string.rep("|", blocks)
    local empty = string.rep(" ", 10 - blocks)
    return "[" .. filled .. empty .. "] " .. tostring(value) .. "%"
end

-- ===========================================================================
-- Settings._render(ctx) — draw settings menu text
-- ===========================================================================
function Settings._render(ctx)
    if not state.active then return end
    local w, h = backend.get_resolution()
    w = w or 1280
    h = h or 720
    local px = state.panelX or 340
    local py = state.panelY or 120
    local pw = state.panelW or 600
    local ph = state.panelH or 360

    local startY = py + 50
    local lineH = 30

    -- Title: "Settings / 設置" at top of panel
    local titleStr = "Settings / 設置"
    local titleX = px + (pw - #titleStr * 9) / 2  -- rough centering
    backend.render_text(titleStr, titleX, py + 12, 255, 200, 60, 255)

    -- Title separator line
    backend.render_text(string.rep("-", math.floor(pw / 9)), px + 20, py + 28, 80, 80, 140, 200)

    -- Menu items
    for i, item in ipairs(state.items) do
        local lineY = startY + (i - 1) * lineH
        local isSelected = (i == state.cursor)
        local prefix = isSelected and " > " or "   "

        -- Build value display string
        local valStr = ""
        if item.type == "slider" then
            valStr = " " .. Settings._renderVolumeBar(item.value, item.max or 100)
        elseif item.type == "toggle" then
            valStr = item.value and " [ON]" or " [OFF]"
        elseif item.type == "cycle" then
            valStr = " <" .. tostring(item.value) .. ">"
        end

        local line = prefix .. item.label .. valStr
        local r, g, b = 220, 220, 255  -- default text color
        if isSelected then
            r, g, b = 255, 255, 150  -- yellow for selected
        elseif item.type == "action" then
            r, g, b = 180, 180, 200  -- dim for action items
        elseif item.type == "toggle" and item.value then
            r, g, b = 100, 255, 100  -- green for ON toggles
        end
        backend.render_text(line, px + 30, lineY, r, g, b, 255)
    end

    -- Cursor highlight position (position behind the selected row)
    if state.cursorLayer then
        state.cursorLayer.x = px + 26
        state.cursorLayer.y = startY + (state.cursor - 1) * lineH - 2
    end

    -- Footer inside panel
    local footerY = py + ph - 24
    backend.render_text("Arrow Keys: Navigate | Left/Right: Adjust | Enter: Confirm | ESC: Back", px + 20, footerY, 150, 150, 200, 255)
end

-- ===========================================================================
-- Settings._applyAll(ctx) — apply all settings values
-- ===========================================================================
function Settings._applyAll(ctx)
    local sv = ctx.settingsValues
    if not sv then return end
    if sv.volume_bgm then audio.set_bgm_volume(sv.volume_bgm / 100) end
    if sv.volume_se then audio.set_se_volume(sv.volume_se / 100) end
    -- Keys must match the runner/commands (snake_case); camelCase variants
    -- were silently ignored by kag_runner/TextCommands.
    if sv.text_speed then ctx.text_speed = sv.text_speed end
    if sv.skip_mode ~= nil then ctx.skip_mode = sv.skip_mode end
    if sv.skip_auto ~= nil then ctx.skip_auto = sv.skip_auto end
    if sv.auto_mode ~= nil then ctx.auto_mode = sv.auto_mode end
    if sv.volume_voice then audio.set_voice_volume(sv.volume_voice / 100) end
    if sv.fullscreen ~= nil then backend.set_fullscreen(sv.fullscreen) end
end

-- ===========================================================================
-- Settings.navigate(ctx, direction) — move cursor Up/Down
-- ===========================================================================
function Settings.navigate(ctx, direction)
    if not state.active then return end
    if direction == "up" then
        state.cursor = state.cursor - 1
        if state.cursor < 1 then state.cursor = #state.items end
    elseif direction == "down" then
        state.cursor = state.cursor + 1
        if state.cursor > #state.items then state.cursor = 1 end
    end
    Settings._render(ctx)
end

-- ===========================================================================
-- Settings.adjust(ctx, direction) — adjust selected value Left/Right
-- ===========================================================================
function Settings.adjust(ctx, direction)
    if not state.active then return end
    local item = state.items[state.cursor]
    if not item then return end
    local sv = ctx.settingsValues

    if item.type == "slider" then
        local step = item.step or 5
        if direction == "left" then
            item.value = math.max(item.min, item.value - step)
        else
            item.value = math.min(item.max, item.value + step)
        end
        sv[item.key] = item.value
        -- Live preview
        if item.key == "volume_bgm" then audio.set_bgm_volume(item.value / 100)
        elseif item.key == "volume_se" then audio.set_se_volume(item.value / 100)
        end

    elseif item.type == "toggle" then
        item.value = not item.value
        sv[item.key] = item.value

    elseif item.type == "cycle" then
        local opts = item.options or {}
        if #opts > 0 then
            local curIdx = 1
            for i, o in ipairs(opts) do if o == item.value then curIdx = i; break end end
            if direction == "left" then curIdx = curIdx - 1 else curIdx = curIdx + 1 end
            if curIdx < 1 then curIdx = #opts end
            if curIdx > #opts then curIdx = 1 end
            item.value = opts[curIdx]
            sv[item.key] = item.value
            if item.key == "language" then
                -- Update label for display
                item.label = i18n.t("language") .. ": " .. item.value
                -- Hot-switch language
                i18n.load(item.value)
                -- Rebuild menu items with new translations
                state.items = Settings._buildMenu(ctx)
            elseif item.key == "autosave_interval" then
                item.label = i18n.t("autosave_interval") .. ": " ..
                    (item.value > 0 and item.value .. "s" or "Off")
                -- Apply the interval to the engine timer
                pcall(function()
                    local engine = rawget(_G, "_CAESURA_ENGINE")
                    if engine and engine.setAutoSaveInterval then
                        engine:setAutoSaveInterval(item.value)
                    end
                end)
            end
        end
    end
    Settings._render(ctx)
end

-- ===========================================================================
-- Settings.confirm(ctx) — activate selected item (Enter/Click)
-- ===========================================================================
function Settings.confirm(ctx)
    if not state.active then return end
    local item = state.items[state.cursor]
    if not item then return end

    if item.type == "action" and item.key == "back" then
        Settings.hide(ctx)
        return
    end
    -- Toggle items also activate on confirm
    if item.type == "toggle" then
        Settings.adjust(ctx, "right")
    end
end

-- ===========================================================================
-- Settings.onClick(ctx, x, y) — handle click in settings menu
-- ===========================================================================
function Settings.onClick(ctx, x, y)
    if not state.active then return false end
    -- Map y coordinate to menu item within the panel
    local px = state.panelX or 340
    local py = state.panelY or 120
    local startY = py + 50
    local lineH = 30
    local clickedIdx = math.floor((y - startY) / lineH) + 1
    if clickedIdx >= 1 and clickedIdx <= #state.items then
        state.cursor = clickedIdx
        Settings.confirm(ctx)
    end
    return true  -- consumed
end

return Settings
