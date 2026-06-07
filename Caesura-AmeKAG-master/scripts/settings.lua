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
}

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
    auto_mode    = false,
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
        {label = i18n.t("auto_mode"),    key = "auto_mode",    type = "toggle", value = sv.auto_mode},
        {label = i18n.t("fullscreen"),   key = "fullscreen",   type = "toggle", value = sv.fullscreen},
        {label = i18n.t("language") .. ": " .. (i18n.current or "zh"),
         key = "language", type = "cycle", value = i18n.current,
         options = i18n.available()},
        {label = i18n.t("back"), key = "back", type = "action"},
    }
    return items
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

    -- Create dimmed background layer
    local w, h = backend.get_resolution()
    local bg = layers.ensure(ctx, "_settings_bg", 95)
    bg.visible = true
    bg.x, bg.y = 0, 0
    bg.w, bg.h = w or 1280, h or 720
    bg.texture = backend.create_solid_texture(0, 0, 0, 180)  -- semi-transparent dark
    state.bgLayer = bg

    -- Create cursor highlight layer
    local cursor = layers.ensure(ctx, "_settings_cursor", 96)
    cursor.visible = true
    cursor.texture = backend.create_solid_texture(255, 255, 255, 60)
    cursor.w = 600
    cursor.h = 30
    state.cursorLayer = cursor

    Settings._render(ctx)
    print("[Settings] Menu opened. Up/Down to navigate, Left/Right to adjust, Enter/Click to confirm.")
end

-- ===========================================================================
-- Settings.hide(ctx) — close settings, restore KAG focus
-- ===========================================================================
function Settings.hide(ctx)
    state.active = false
    ctx._settingsActive = false
    for _, name in ipairs({"_settings_bg", "_settings_cursor"}) do
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
    print("[Settings] Menu closed.")
end

-- ===========================================================================
-- Settings._render(ctx) — draw settings menu text
-- ===========================================================================
function Settings._render(ctx)
    if not state.active then return end
    local w, h = backend.get_resolution()
    local startY = 120
    local lineH = 32

    -- Title
    backend.render_text("=== " .. i18n.t("settings") .. " ===", 400, startY - 40)

    -- Menu items
    for i, item in ipairs(state.items) do
        local prefix = (i == state.cursor) and " > " or "   "
        local valStr = ""
        if item.type == "slider" then
            valStr = " [" .. string.rep("#", math.floor(item.value / 10)) .. string.rep("-", 10 - math.floor(item.value / 10)) .. "] " .. tostring(item.value)
        elseif item.type == "toggle" then
            valStr = item.value and " [ON]" or " [OFF]"
        elseif item.type == "cycle" then
            valStr = " <" .. tostring(item.value) .. ">"
        end
        local line = prefix .. item.label .. valStr
        backend.render_text(line, 300, startY + (i - 1) * lineH)
    end

    -- Cursor position indicator
    if state.cursorLayer then
        state.cursorLayer.x = 290
        state.cursorLayer.y = startY + (state.cursor - 1) * lineH - 2
    end
end

-- ===========================================================================
-- Settings._applyAll(ctx) — apply all settings values
-- ===========================================================================
function Settings._applyAll(ctx)
    local sv = ctx.settingsValues
    if not sv then return end
    if sv.volume_bgm then audio.set_bgm_volume(sv.volume_bgm / 100) end
    if sv.volume_se then audio.set_se_volume(sv.volume_se / 100) end
    if sv.text_speed then ctx.textSpeed = sv.text_speed end
    if sv.skip_mode ~= nil then ctx.skipMode = sv.skip_mode end
    if sv.auto_mode ~= nil then ctx.autoMode = sv.auto_mode end
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
            -- Update label for display
            item.label = i18n.t("language") .. ": " .. item.value
            -- Hot-switch language
            i18n.load(item.value)
            -- Rebuild menu items with new translations
            state.items = Settings._buildMenu(ctx)
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
    -- Map y coordinate to menu item
    local w, h = backend.get_resolution()
    local startY = 120
    local lineH = 32
    local clickedIdx = math.floor((y - startY) / lineH) + 1
    if clickedIdx >= 1 and clickedIdx <= #state.items then
        state.cursor = clickedIdx
        Settings.confirm(ctx)
    end
    return true  -- consumed
end

return Settings
