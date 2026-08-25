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

-- Continue (slot 0 / autosave) is offered only when a save actually
-- exists: polled once on entry via KAG.save_exists (C++ storage layer).
local hasAutosave = (type(KAG) == "table" and KAG.save_exists and KAG.save_exists(0)) or false
local MENU_ITEMS = { "new_game", hasAutosave and "continue" or nil, "load_game", "settings", "endings", "exit" }
for i = #MENU_ITEMS, 1, -1 do
    if MENU_ITEMS[i] == nil then table.remove(MENU_ITEMS, i) end
end

--- TitleMenu.showEndings(ctx) — inline sub-screen (no separate coroutine:
-- keeps the same yield-loop pattern as the title itself, so no orphan
-- coroutine soft-lock like the gallery/music experiment). Lists unlocked
-- endings from ctx.seen_endings ([ending id=] records them).
local function showEndings(ctx)
    local endings = ctx.seen_endings
    if type(endings) ~= "table" then endings = {} end
    local names = {}
    local scenes = {}
    for id, info in pairs(endings) do
        local n = (type(info) == "table" and info.name) or id
        names[#names + 1] = n
        scenes[n] = (type(info) == "table" and info.scene) or ""
    end
    table.sort(names)
    local cursor = 1
    local vw, vh = require("viewport").wh()  -- viewported from 1280x720
    while true do
        backend.render_text(i18n.t("endings_title") or "Endings", math.floor(vw / 2) - 60, 180, 220, 180, 80, 255)
        if #names == 0 then
            backend.render_text("(none yet)", math.floor(vw / 2) - 60, 300, 120, 120, 160, 255)
        else
            local y = 300
            for i, n in ipairs(names) do
                local prefix = (i == cursor) and "> " or "  "
                local r, g, b = 255, 255, 255
                if i == cursor then r, g, b = 255, 220, 80 end
                backend.render_text(prefix .. "[OK] " .. n, math.floor(vw / 2) - 80, y, r, g, b, 255)
                y = y + 40
            end
        end
        backend.render_text("[Up/Down] Select  [Enter] Replay  [Esc] Back", 20, vh - 30, 120, 120, 160, 255)
        coroutine.yield()
        if _G._GAME_KEY_UP == true then
            _G._GAME_KEY_UP = false
            cursor = cursor - 1
            if cursor < 1 then cursor = #names end
        elseif _G._GAME_KEY_DOWN == true then
            _G._GAME_KEY_DOWN = false
            cursor = cursor + 1
            if cursor > #names then cursor = 1 end
        elseif _G._GAME_KEY_ENTER == true then
            _G._GAME_KEY_ENTER = false
            local target = scenes[names[cursor]]
            if target and #target > 0 then
                -- Replay: jump to the ending scene (runner handles the
                -- cross-scene load via _pendingJump).
                ctx._pendingJump = { scene = target }
                return
            end
        elseif _G._GAME_KEY_ESC == true then
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
    local vw, vh = require("viewport").wh()  -- viewported from 1280x720
    bg.w, bg.h = vw, vh
    bg.texture = solid(8, 8, 20, 255)

    local cursor = 1
    while true do
        -- Title
        backend.render_text(i18n.t("title_screen"), math.floor(vw / 2) - 120, 180, 220, 180, 80, 255)

        -- Menu items
        local y = 320
        for i, key in ipairs(MENU_ITEMS) do
            local label = i18n.t(key)
            if i == cursor then
                backend.render_text("> " .. label, math.floor(vw / 2) - 80, y, 255, 220, 80, 255)
            else
                backend.render_text("  " .. label, math.floor(vw / 2) - 80, y, 180, 180, 200, 255)
            end
            y = y + 44
        end
        backend.render_text("[Up/Down] Select  [Enter] Confirm  [Esc] Exit", 20, vh - 30, 120, 120, 160, 255)

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
            elseif key == "continue" then return "continue"
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
