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

-- Helper: safe texture creation
local function solid(r, g, b, a)
    return backend.create_solid_texture(math.floor(r), math.floor(g), math.floor(b), math.floor(a or 255))
end

-- Help textures owned by this module (cleaned on hide)
local ownedTextures = {}

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
-- Gallery.isUnlocked(ctx, id) -> bool
-- ===========================================================================
function Gallery.isUnlocked(ctx, id)
    if not ctx.unlockedCG then return false end
    return ctx.unlockedCG[id] == true
end

-- ===========================================================================
-- Gallery._cleanupTextures() — destroy all module-owned textures
-- ===========================================================================
function Gallery._cleanupTextures()
    for _, texId in pairs(ownedTextures) do
        if texId and texId > 0 then
            pcall(function() backend.destroy_texture(texId) end)
        end
    end
    ownedTextures = {}
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
        overlayLayer = nil,
        flashTimer = 0,     -- navigation flash indicator
    }

    -- Switch input focus to GAME to prevent KAG text advancement
    backend.set_input_focus("GAME")

    local w, h = backend.get_resolution()
    w = w or 1280
    h = h or 720

    -- Layer 1: Dark semi-transparent full-screen overlay background
    local overlayTex = solid(0, 0, 0, 180)
    ownedTextures["_gallery_overlay"] = overlayTex
    local overlay = layers.ensure(ctx, "_gallery_overlay", 95)
    overlay.visible = true
    overlay.x, overlay.y = 0, 0
    overlay.w, overlay.h = w, h
    overlay.texture = overlayTex
    ctx.galleryState.overlayLayer = overlay

    -- Layer 2: CG image layer (drawn on top of overlay)
    local layer = layers.ensure(ctx, "_gallery", 100)
    layer.visible = true
    layer.x, layer.y = 0, 0
    layer.w, layer.h = w, h
    ctx.galleryState.bgLayer = layer

    Gallery._renderCurrent(ctx)
    print("[Gallery] Opened. Left/Right to navigate. Click to close.")
end

-- ===========================================================================
-- Gallery.hide(ctx)
-- ===========================================================================
function Gallery.hide(ctx)
    if not ctx.galleryState then return end
    local layerNames = {"_gallery", "_gallery_overlay"}
    for _, name in ipairs(layerNames) do
        local layer = layers.find(name)
        if layer then
            layer.visible = false
            if layer.texture then backend.destroy_texture(layer.texture); layer.texture = nil end
        end
    end
    Gallery._cleanupTextures()
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

    local w, h = backend.get_resolution()
    w = w or 1280
    h = h or 720

    -- Load CG texture (centered, with padding)
    local tex = backend.load_texture(cg.path)
    if tex and tex > 0 then
        if layer.texture then backend.destroy_texture(layer.texture) end
        layer.texture = tex
        -- Center CG with 60px padding from edges
        local padW = 60
        local padH = 80  -- extra top/bottom for title/footer
        layer.x = padW
        layer.y = padH
        layer.w = w - padW * 2
        layer.h = h - padH - 80
    else
        -- Fallback: colored placeholder
        local hash = 0
        for c in cg.id:gmatch(".") do hash = (hash * 31 + c:byte()) % 16777216 end
        local r, g, b = (hash >> 16) & 0xFF, (hash >> 8) & 0xFF, hash & 0xFF
        if layer.texture then backend.destroy_texture(layer.texture); layer.texture = nil end
        layer.texture = solid(r, g, b, 255)
        layer.x, layer.y = 60, 80
        layer.w, layer.h = w - 120, h - 160
    end

    -- Title bar: "CG Gallery" on top
    backend.render_text("CG Gallery", 32, 16, 255, 200, 60, 255)

    -- CG name and unlock status
    local statusStr = "[Unlocked]"
    local statusR, statusG, statusB = 60, 220, 100  -- green for unlocked
    backend.render_text(statusStr, 200, 16, statusR, statusG, statusB, 255)
    backend.render_text(cg.name, 340, 16, 255, 255, 255, 255)

    -- Page indicator: "CG 3 / 12"
    local pageStr = "CG " .. gs.index .. " / " .. #gs.cgs
    backend.render_text(pageStr, w - 220, 16, 200, 200, 255, 255)

    -- Bottom navigation hints
    local footerY = h - 36
    backend.render_text("Left/Right: Navigate | ESC/Click: Close", 32, footerY, 180, 180, 200, 255)
    backend.render_text(pageStr, w - 220, footerY, 180, 180, 200, 255)

    -- Navigation flash indicator (brief highlight on navigate)
    if gs.flashTimer and gs.flashTimer > 0 then
        local flashAlpha = math.floor(80 * (gs.flashTimer / 0.15))  -- fade over 150ms
        backend.render_text("< >", w / 2 - 20, h / 2, 255, 255, 255, flashAlpha)
    end

    -- Also render caption in classic position (backward compat)
    backend.render_text(cg.name .. " (" .. gs.index .. "/" .. #gs.cgs .. ")", 32, h - 60, 160, 160, 180, 255)
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
    -- Set flash timer for smooth navigation feedback
    gs.flashTimer = 0.15
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
