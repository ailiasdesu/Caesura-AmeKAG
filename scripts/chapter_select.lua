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
local Operation = require("kag.operation")

local function solid(r, g, b, a)
    return backend.create_solid_texture(math.floor(r), math.floor(g), math.floor(b), math.floor(a or 255))
end

--- ChapterSelect.collect(ctx) → { {label, name}, ... } sorted by name
function ChapterSelect.collect(ctx)
    local out = {}
    if not ctx or not ctx.labelMap then return out end
    -- A chapter is "read" when its scene appears in seen_scenes (the
    -- crafted-save-validated set the runner maintains) OR the label was
    -- actually visited this session (labelMap is built from the loaded
    -- scene's labels; mark labels already jumped to via _visited_labels).
    local seen = ctx.seen_scenes
    local visited = ctx._visited_labels
    if type(seen) ~= "table" then seen = {} end
    if type(visited) ~= "table" then visited = {} end
    for label in pairs(ctx.labelMap) do
        if label:match("^chapter_") or label:match("^chapter%d") then
            local name = label:gsub("^chapter_", ""):gsub("^chapter", "第")
            local sceneName = ctx.current_scene or ""
            local read = visited[label] == true
                or (type(seen[sceneName]) == "number" and seen[sceneName] > 0)
                or seen[sceneName] == true
            table.insert(out, { label = label, name = name, read = read })
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

    local scope <close> = Operation.start(ctx)
    local previousFocus = ctx.input_focus or "kag"
    local ok, focus = pcall(backend.get_input_focus)
    local previousBackendFocus = ok and type(focus) == "string" and focus
        or (previousFocus == "kag" and "KAG" or "GAME")
    local bg, texture, cleaned
    local function cleanup()
        if cleaned then return end
        cleaned = true
        if bg then bg.visible, bg.texture = false, nil end
        if texture then pcall(backend.destroy_texture, texture) end
        if ctx.input_focus == "chapter" then
            ctx.input_focus = previousFocus
            for _, key in ipairs({"UP", "DOWN", "ENTER", "ESC"}) do
                _G["_GAME_KEY_" .. key] = false
            end
            pcall(backend.set_input_focus, previousBackendFocus)
        end
    end
    local function finish(result)
        cleanup()
        scope:complete()
        return result
    end
    scope.token:register(cleanup)
    ctx.input_focus = "chapter"
    backend.set_input_focus("GAME")

    bg = layers.ensure(ctx, "_chapter_bg", 190)
    bg.visible = true
    local vw, vh = require("viewport").wh()  -- viewported from 1280x720
    bg.x, bg.y, bg.w, bg.h = 0, 0, vw, vh
    texture = solid(0, 0, 0, 210)
    bg.texture = texture

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
            -- Read badge: ✓ read / · new (seen_scenes-driven, per collect)
            local badge = ch.read and "[OK] " or "[..] "
            backend.render_text(prefix .. badge .. ch.name, 30, y, r, g, b, 255)
            y = y + 44
        end
        backend.render_text("[Up/Down] Select  [Enter] Jump  [Esc] Cancel", 20, vh - 30, 120, 120, 160, 255)
        coroutine.yield()
        if scope.token.cancelled then return end

        if _G._GAME_KEY_UP == true then
            _G._GAME_KEY_UP = false
            cursor = math.max(1, cursor - 1)
        elseif _G._GAME_KEY_DOWN == true then
            _G._GAME_KEY_DOWN = false
            cursor = math.min(#chapters, cursor + 1)
        elseif _G._GAME_KEY_ENTER == true then
            _G._GAME_KEY_ENTER = false
            bg.visible = false
            return finish(chapters[cursor].label)
        elseif _G._GAME_KEY_ESC == true then
            _G._GAME_KEY_ESC = false
            bg.visible = false
            return finish(nil)
        end
        if cursor <= scroll then scroll = cursor - 1
        elseif cursor >= scroll + ITEMS then scroll = cursor - ITEMS + 1 end
        scroll = math.max(0, math.min(#chapters - ITEMS, scroll))
    end
end

return ChapterSelect
