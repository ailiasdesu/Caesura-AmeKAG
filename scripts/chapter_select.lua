-- ===========================================================================
--  chapter_select.lua — Chapter selection overlay (KiriKiri ChapterSelect)
--  Scans the current scene's labels for "*chapter_" entries, shows a
--  scrollable list, and jumps to the chosen label. Run via the [chapter]
--  KAG command (scheduler coroutine drives the per-frame yield) -- never
--  from a bare coroutine.create + single resume (orphan soft-lock).
-- ===========================================================================

local ChapterSelect = {}
local backend = require("backend")
local layers = require("layers")

local function solid(r, g, b, a)
    return backend.create_solid_texture(math.floor(r), math.floor(g), math.floor(b), math.floor(a or 255))
end

--- ChapterSelect.collect(ctx) → { {label, name}, ... } sorted by name
function ChapterSelect.collect(ctx)
    local out = {}
    if not ctx or not ctx.labelMap then return out end
    for label in pairs(ctx.labelMap) do
        if label:match("^chapter_") or label:match("^chapter%d") then
            local name = label:gsub("^chapter_", ""):gsub("^chapter", "第")
            table.insert(out, { label = label, name = name })
        end
    end
    table.sort(out, function(a, b) return a.label < b.label end)
    return out
end

--- ChapterSelect.show(ctx) — blocking overlay; returns the chosen label or nil.
function ChapterSelect.show(ctx)
    local chapters = ChapterSelect.collect(ctx)
    if #chapters == 0 then
        print("[ChapterSelect] No *chapter_* labels in the current scene.")
        return nil
    end

    local bg = layers.ensure(ctx, "_chapter_bg", 190)
    bg.visible = true
    bg.x, bg.y, bg.w, bg.h = 0, 0, 1280, 720
    bg.texture = solid(0, 0, 0, 210)

    local cursor, scroll, ITEMS = 1, 0, 12
    while true do
        backend.render_text("Chapter Select / 章节选择", 20, 8, 255, 200, 60, 255)
        local y = 52
        local last = math.min(scroll + ITEMS, #chapters)
        for i = scroll + 1, last do
            local ch = chapters[i]
            local prefix = (i == cursor) and "> " or "  "
            local r, g, b = 255, 255, 255
            if i == cursor then r, g, b = 255, 220, 80 end
            backend.render_text(prefix .. ch.name, 30, y, r, g, b, 255)
            y = y + 44
        end
        backend.render_text("[Up/Down] Select  [Enter] Jump  [Esc] Cancel", 20, 690, 120, 120, 160, 255)
        coroutine.yield()

        if _G._GAME_KEY_UP == true then
            _G._GAME_KEY_UP = false
            cursor = math.max(1, cursor - 1)
        elseif _G._GAME_KEY_DOWN == true then
            _G._GAME_KEY_DOWN = false
            cursor = math.min(#chapters, cursor + 1)
        elseif _G._GAME_KEY_ENTER == true then
            _G._GAME_KEY_ENTER = false
            bg.visible = false
            return chapters[cursor].label
        elseif _G._GAME_KEY_ESC == true then
            _G._GAME_KEY_ESC = false
            bg.visible = false
            return nil
        end
        if cursor <= scroll then scroll = cursor - 1
        elseif cursor >= scroll + ITEMS then scroll = cursor - ITEMS + 1 end
        scroll = math.max(0, math.min(#chapters - ITEMS, scroll))
    end
end

return ChapterSelect
