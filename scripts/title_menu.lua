-- ===========================================================================
--  title_menu.lua — Title screen (VN standard UI, differentiator)
--  New Game / Load Game / Settings / Exit. i18n tri-lingual (zh/en/ja),
--  layer-based UI (settings.lua pattern), keyboard + mouse navigation.
--  Call TitleMenu.show(ctx) from a Lua entry; returns an action:
--    "new", "load", "settings", "exit", or nil when dismissed.
-- ===========================================================================

local TitleMenu = {}
local backend = require("backend")
local layers = require("layers")
local i18n = require("i18n")

local function solid(r, g, b, a)
    return backend.create_solid_texture(math.floor(r), math.floor(g), math.floor(b), math.floor(a or 255))
end

local MENU_ITEMS = { "new_game", "load_game", "settings", "endings", "exit" }

--- TitleMenu.showEndings(ctx) — inline sub-screen (no separate coroutine:
-- keeps the same yield-loop pattern as the title itself, so no orphan
-- coroutine soft-lock like the gallery/music experiment). Lists unlocked
-- endings from ctx.seen_endings ([ending id=] records them).
local function showEndings(ctx)
    local endings = ctx.seen_endings
    if type(endings) ~= "table" then endings = {} end
    local names = {}
    for id, info in pairs(endings) do
        names[#names + 1] = (type(info) == "table" and info.name) or id
    end
    table.sort(names)
    while true do
        backend.render_text(i18n.t("endings_title") or "Endings", 1280 / 2 - 60, 180, 220, 180, 80, 255)
        if #names == 0 then
            backend.render_text("(none yet)", 1280 / 2 - 60, 300, 120, 120, 160, 255)
        else
            local y = 300
            for _, n in ipairs(names) do
                backend.render_text("[OK] " .. n, 1280 / 2 - 80, y, 180, 180, 200, 255)
                y = y + 40
            end
        end
        backend.render_text("[Esc] Back", 20, 690, 120, 120, 160, 255)
        coroutine.yield()
        if _G._GAME_KEY_ESC == true then
            _G._GAME_KEY_ESC = false
            return
        end
    end
end

--- TitleMenu.show(ctx) → "new"|"load"|"settings"|"exit"|nil
function TitleMenu.show(ctx)
    local bg = layers.ensure(ctx, "_title_bg", 90)
    bg.visible = true
    bg.x, bg.y = 0, 0
    bg.w, bg.h = 1280, 720
    bg.texture = solid(8, 8, 20, 255)

    local cursor = 1
    while true do
        -- Title
        backend.render_text(i18n.t("title_screen"), 1280 / 2 - 120, 180, 220, 180, 80, 255)

        -- Menu items
        local y = 320
        for i, key in ipairs(MENU_ITEMS) do
            local label = i18n.t(key)
            if i == cursor then
                backend.render_text("> " .. label, 1280 / 2 - 80, y, 255, 220, 80, 255)
            else
                backend.render_text("  " .. label, 1280 / 2 - 80, y, 180, 180, 200, 255)
            end
            y = y + 44
        end
        backend.render_text("[Up/Down] Select  [Enter] Confirm  [Esc] Exit", 20, 690, 120, 120, 160, 255)

        coroutine.yield()

        -- Input (polled one-shot globals)
        if _G._GAME_KEY_UP == true then
            _G._GAME_KEY_UP = false
            cursor = cursor - 1
            if cursor < 1 then cursor = #MENU_ITEMS end
        elseif _G._GAME_KEY_DOWN == true then
            _G._GAME_KEY_DOWN = false
            cursor = cursor + 1
            if cursor > #MENU_ITEMS then cursor = 1 end
        elseif _G._GAME_KEY_ENTER == true then
            _G._GAME_KEY_ENTER = false
            bg.visible = false
            local key = MENU_ITEMS[cursor]
            if key == "new_game" then return "new"
            elseif key == "load_game" then return "load"
            elseif key == "settings" then return "settings"
            elseif key == "endings" then showEndings(ctx)
            else return "exit" end
        elseif _G._GAME_KEY_ESC == true then
            _G._GAME_KEY_ESC = false
            bg.visible = false
            return nil
        end
    end
end

return TitleMenu
