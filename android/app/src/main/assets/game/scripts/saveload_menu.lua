-- ===========================================================================
--  saveload_menu.lua — Save/Load slot UI (KiriKiri-style, scheduler-driven)
--  Lists save slots (filled + empty), lets the player pick one to save into
--  or load from. Run via the [saveload mode=save|load] KAG command so the
--  scheduler coroutine drives the per-frame yield (never a bare resume).
-- ===========================================================================

local SaveLoad = {}
local backend = require("backend")
local layers = require("layers")
local i18n = require("i18n")

local NUM_SLOTS = 10

local function solid(r, g, b, a)
    return backend.create_solid_texture(math.floor(r), math.floor(g), math.floor(b), math.floor(a or 255))
end

--- SaveLoad.show(ctx, mode) — mode "save" or "load"; returns {slot=, action=} or nil.
function SaveLoad.show(ctx, mode)
    mode = mode or "save"
    local bg = layers.ensure(ctx, "_saveload_bg", 192)
    bg.visible = true
    bg.x, bg.y, bg.w, bg.h = 0, 0, 1280, 720
    bg.texture = solid(0, 0, 0, 210)

    local slots = require("kag.commands.save").listsaves(ctx, {})
    -- slots is a list of {slot, desc, has_data} or similar
    local meta = {}
    if type(slots) == "table" then
        for _, s in ipairs(slots) do
            if s and s.slot then meta[s.slot] = s end
        end
    end

    local cursor, scroll, ITEMS = 1, 0, 8
    while true do
        local title = (mode == "save") and "Save / 保存" or "Load / 读档"
        backend.render_text(title, 20, 8, 255, 200, 60, 255)
        local y = 52
        for i = scroll + 1, math.min(scroll + ITEMS, NUM_SLOTS) do
            local m = meta[i]
            local label = m and (m.desc ~= "" and m.desc or ("Slot " .. i)) or ("Slot " .. i .. " (empty)")
            local prefix = (i == cursor) and "> " or "  "
            local r, g, b = 255, 255, 255
            if i == cursor then r, g, b = 255, 220, 80 end
            backend.render_text(prefix .. label, 40, y, r, g, b, 255)
            y = y + 52
        end
        backend.render_text("[Up/Down] Select  [Enter] Confirm  [Esc] Cancel", 20, 690, 120, 120, 160, 255)
        coroutine.yield()

        if _G._GAME_KEY_UP == true then
            _G._GAME_KEY_UP = false
            cursor = math.max(1, cursor - 1)
        elseif _G._GAME_KEY_DOWN == true then
            _G._GAME_KEY_DOWN = false
            cursor = math.min(NUM_SLOTS, cursor + 1)
        elseif _G._GAME_KEY_ENTER == true then
            _G._GAME_KEY_ENTER = false
            bg.visible = false
            return { slot = cursor, action = mode }
        elseif _G._GAME_KEY_ESC == true then
            _G._GAME_KEY_ESC = false
            bg.visible = false
            return nil
        end
        if cursor <= scroll then scroll = cursor - 1
        elseif cursor >= scroll + ITEMS then scroll = cursor - ITEMS + 1 end
        scroll = math.max(0, math.min(NUM_SLOTS - ITEMS, scroll))
    end
end

return SaveLoad
