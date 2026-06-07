-- ===========================================================================
--  Caesura (AmeKAG) — gallery.lua  [P1.1]
--  CG Gallery: browse unlocked CGs with fullscreen view + left/right navigation.
--  Unlock state persisted in save data (ctx.unlockedCG).
-- ===========================================================================

local Gallery = {}
local backend = require("backend")
local layers  = require("layers")
local fileutil = require("fileutil")

-- Cache of scanned CG entries
local cgCache = nil

-- ===========================================================================
-- Gallery.scan(ctx) -> { {id, path, name}, ... }
-- Scans assets/cg/ for .png/.dds/.jpg files. Cross-platform (W7).
-- ===========================================================================
function Gallery.scan(ctx)
    if cgCache then return cgCache end
    cgCache = {}
    local dirs = {"assets/cg/", "assets/cg", "data/cg/", "data/cg"}
    local pattern = "%.png$|%.dds$|%.jpg$|%.jpeg$"
    for _, dir in ipairs(dirs) do
        local files = fileutil.scan_dir(dir, pattern)
        if #files > 0 then
            for _, fname in ipairs(files) do
                local id = fname:match("^(.-)%.[^.]+$") or fname
                table.insert(cgCache, {id = id, path = dir .. "/" .. fname, name = id})
            end
            break
        end
    end
    table.sort(cgCache, function(a, b) return a.id < b.id end)
    return cgCache
end

-- ===========================================================================
-- Gallery.list() -> {id, name, unlocked}
-- ===========================================================================
function Gallery.list(ctx)
    local cgs = Gallery.scan(ctx)
    ctx.unlockedCG = ctx.unlockedCG or {}
    local result = {}
    for _, cg in ipairs(cgs) do
        table.insert(result, {
            id = cg.id,
            name = cg.name,
            unlocked = ctx.unlockedCG[cg.id] == true,
            path = cg.path,
        })
    end
    return result
end

-- ===========================================================================
-- Gallery.unlock(id) — mark CG as unlocked in ctx
-- ===========================================================================
function Gallery.unlock(ctx, id)
    if not id then return end
    ctx.unlockedCG = ctx.unlockedCG or {}
    ctx.unlockedCG[id] = true
    print("[Gallery] Unlocked: " .. id)
end

-- ===========================================================================
-- Gallery.show(ctx) — interactive gallery viewer
-- ===========================================================================
function Gallery.show(ctx, startId)
    local cgs = Gallery.scan(ctx)
    if #cgs == 0 then
        print("[Gallery] No CGs found.")
        return
    end
    ctx.unlockedCG = ctx.unlockedCG or {}

    local idx = 1
    if startId then
        for i, cg in ipairs(cgs) do
            if cg.id == startId then idx = i; break end
        end
    end

    local unlocked = {}
    for _, cg in ipairs(cgs) do
        if ctx.unlockedCG[cg.id] then
            table.insert(unlocked, cg)
        end
    end
    if #unlocked == 0 then
        print("[Gallery] No CGs unlocked yet.")
        return
    end

    ctx.galleryState = {
        active = true,
        cgs = unlocked,
        index = math.max(1, math.min(idx, #unlocked)),
        bgLayer = nil,
    }

    backend.set_input_focus("GAME")

    local layer = layers.ensure(ctx, "_gallery", 100)
    layer.visible = true
    layer.x, layer.y = 0, 0
    local w, h = backend.get_resolution()
    layer.w, layer.h = w or 1280, h or 720
    ctx.galleryState.bgLayer = layer

    Gallery._renderCurrent(ctx)
    print("[Gallery] Opened. Left/Right to navigate. Click to close.")
end

-- ===========================================================================
-- Gallery.hide(ctx)
-- ===========================================================================
function Gallery.hide(ctx)
    if not ctx.galleryState then return end
    local layer = layers.find("_gallery")
    if layer then
        layer.visible = false
        if layer.texture then backend.destroy_texture(layer.texture); layer.texture = nil end
    end
    ctx.galleryState = nil
    backend.set_input_focus("KAG")
    print("[Gallery] Closed.")
end

-- ===========================================================================
-- Gallery._renderCurrent(ctx)
-- ===========================================================================
function Gallery._renderCurrent(ctx)
    local gs = ctx.galleryState
    if not gs then return end
    local cg = gs.cgs[gs.index]
    local layer = gs.bgLayer
    if not layer or not cg then return end

    local tex = backend.load_texture(cg.path)
    if tex and tex > 0 then
        if layer.texture then backend.destroy_texture(layer.texture) end
        layer.texture = tex
    else
        local h = 0
        for c in cg.id:gmatch(".") do h = (h * 31 + c:byte()) % 16777216 end
        local r, g, b = (h >> 16) & 0xFF, (h >> 8) & 0xFF, h & 0xFF
        if layer.texture then backend.destroy_texture(layer.texture); layer.texture = nil end
        layer.texture = backend.create_solid_texture(r, g, b, 255)
    end

    local caption = cg.name .. " (" .. gs.index .. "/" .. #gs.cgs .. ")"
    backend.render_text(caption, 32, 680)
end

-- ===========================================================================
-- Gallery.navigate(ctx, direction)
-- ===========================================================================
function Gallery.navigate(ctx, direction)
    local gs = ctx.galleryState
    if not gs or not gs.active then return end
    if direction == "left" then
        gs.index = gs.index - 1
        if gs.index < 1 then gs.index = #gs.cgs end
    elseif direction == "right" then
        gs.index = gs.index + 1
        if gs.index > #gs.cgs then gs.index = 1 end
    end
    Gallery._renderCurrent(ctx)
end

-- ===========================================================================
-- Gallery.onClick(ctx)
-- ===========================================================================
function Gallery.onClick(ctx, x, y)
    if ctx.galleryState and ctx.galleryState.active then
        Gallery.hide(ctx)
        return true
    end
    return false
end

return Gallery
