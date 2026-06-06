-- =============================================================================
--  Caesura (AmeKAG) — kag/commands/resource.lua
--  Resource lifecycle commands — [preload] tag per spec [10.2.32].
--  Asynchronous asset loading with placeholder textures.
-- =============================================================================

local backend = require("backend")
local layers  = require("layers")

local ResourceCommands = {}

-- ═══════════════════════════════════════════════════════════════════════════
--  Preload state — tracks which assets are loading/loaded
-- ═══════════════════════════════════════════════════════════════════════════

-- texture cache: { path = texture_id }
ResourceCommands._textureCache = {}
-- audio cache:    { path = true }  (audio loaded via backend.audio_play)
ResourceCommands._audioCache   = {}
-- pending loads:  { path = true }  (background, not yet complete)
ResourceCommands._pendingTextures = {}
ResourceCommands._pendingAudio    = {}

-- Placeholder texture IDs (created lazily)
ResourceCommands._placeholderDev     = nil
ResourceCommands._placeholderRelease = nil

-- ═══════════════════════════════════════════════════════════════════════════
--  Placeholder texture generators
--  Dev:    magenta solid (40% alpha) — visually distinct "missing asset"
--  Release: 50% gray semi-transparent — subtle placeholder
--  Note: proper checkerboard requires a shader; solid colors used for now.
-- ═══════════════════════════════════════════════════════════════════════════

local function get_placeholder(is_dev)
    if is_dev then
        if not ResourceCommands._placeholderDev then
            ResourceCommands._placeholderDev = backend.create_solid_texture(180, 0, 180, 160)
        end
        return ResourceCommands._placeholderDev
    else
        if not ResourceCommands._placeholderRelease then
            ResourceCommands._placeholderRelease = backend.create_solid_texture(128, 128, 128, 128)
        end
        return ResourceCommands._placeholderRelease
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Internal: resolve dev/release mode from ctx or config
-- ═══════════════════════════════════════════════════════════════════════════

local function is_dev_mode(ctx)
    if ctx and ctx.dev_mode ~= nil then
        return ctx.dev_mode
    end
    -- Default: dev mode during development
    return true
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Internal: load a single texture, returning its ID (or placeholder on fail)
-- ═══════════════════════════════════════════════════════════════════════════

local function load_texture(path, ctx)
    if ResourceCommands._textureCache[path] then
        return ResourceCommands._textureCache[path]
    end

    -- Mark as pending for background loads
    ResourceCommands._pendingTextures[path] = true

    local tex = backend.load_texture(path)
    if tex then
        ResourceCommands._textureCache[path] = tex
        ResourceCommands._pendingTextures[path] = nil
        return tex
    end

    -- Load failed — return placeholder
    print("[Resource] WARN: texture load failed: " .. path .. " (using placeholder)")
    return get_placeholder(is_dev_mode(ctx))
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Internal: load a single audio file
-- ═══════════════════════════════════════════════════════════════════════════

local function load_audio(path, ctx)
    if ResourceCommands._audioCache[path] then
        return true
    end

    ResourceCommands._pendingAudio[path] = true

    local ok = backend.audio_play and backend.audio_play("se", path)
    if ok then
        -- Immediately stop it (we just wanted to cache the decoded audio)
        backend.audio_stop("se")
        ResourceCommands._audioCache[path] = true
        ResourceCommands._pendingAudio[path] = nil
        return true
    end

    print("[Resource] WARN: audio load failed: " .. path)
    return false
end

-- ═══════════════════════════════════════════════════════════════════════════
--  [preload type="texture|audio" path="bg/01.png,bg/02.png" wait="true|false"]
--
--  Preload assets before they are needed in script.
--
--  type:  "texture" or "audio" — what kind of asset to preload
--  path:  comma-separated list of asset paths (relative to game root)
--  wait:  "true" = block coroutine until all loaded (sync)
--         "false" = load in background, placeholder shown if used early
--
--  Placeholder textures:
--    Dev mode:    magenta solid (RGBA 180,0,180,160) — unmistakable
--    Release mode: 50% gray semi-transparent (RGBA 128,128,128,128) — subtle
--
--  Spec [10.2.32]: Async loading with SDL_PushEvent dispatch (C++ side).
--  Lua side manages cache and placeholder fallback.
-- ═══════════════════════════════════════════════════════════════════════════

function ResourceCommands.preload(ctx, params)
    local assetType = params.type or "texture"
    local pathList  = params.path or params.storage or ""
    local waitMode  = params.wait or "true"
    local isWait    = (waitMode == "true")

    -- Split comma-separated paths
    local paths = {}
    for p in pathList:gmatch("[^,]+") do
        local trimmed = p:match("^%s*(.-)%s*$")  -- trim whitespace
        if #trimmed > 0 then
            table.insert(paths, trimmed)
        end
    end

    if #paths == 0 then
        print("[Resource] preload: no paths specified")
        return
    end

    local dev = is_dev_mode(ctx)
    print(string.format("[Resource] preload type=%s wait=%s dev=%s paths=%d",
        assetType, waitMode, tostring(dev), #paths))

    if assetType == "texture" then
        for _, path in ipairs(paths) do
            local tex = load_texture(path, ctx)
            if isWait and not tex then
                print("[Resource] preload: sync load failed for " .. path)
            end
        end

    elseif assetType == "audio" then
        for _, path in ipairs(paths) do
            local ok = load_audio(path, ctx)
            if isWait and not ok then
                print("[Resource] preload: sync load failed for " .. path)
            end
        end

    else
        print("[Resource] preload: unknown type '" .. assetType .. "'")
    end

    -- If wait mode, yield once to allow rendering the current frame
    if isWait then
        coroutine.yield()
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  ResourceCommands.get_texture(path, ctx) → texture_id
--    Retrieve a preloaded texture (or load it now with placeholder fallback).
--    Called by layer commands when they need a texture that may have been
--    preloaded via [preload wait="false"].
-- ═══════════════════════════════════════════════════════════════════════════

function ResourceCommands.get_texture(path, ctx)
    return load_texture(path, ctx)
end

-- ═══════════════════════════════════════════════════════════════════════════
--  ResourceCommands.is_loaded(path) → bool
--    Check whether a texture is fully loaded (not pending).
-- ═══════════════════════════════════════════════════════════════════════════

function ResourceCommands.is_loaded(path)
    return ResourceCommands._textureCache[path] ~= nil
end

-- ═══════════════════════════════════════════════════════════════════════════
--  ResourceCommands.is_pending(path) → bool
--    Check whether a texture was requested but not yet loaded.
-- ═══════════════════════════════════════════════════════════════════════════

function ResourceCommands.is_pending(path)
    return ResourceCommands._pendingTextures[path] == true
end

-- ═══════════════════════════════════════════════════════════════════════════
--  ResourceCommands.flush_cache()
--    Clear all preloaded texture/audio caches.
--    Called on [link] / scene transitions to free memory.
-- ═══════════════════════════════════════════════════════════════════════════

function ResourceCommands.flush_cache()
    for path, texId in pairs(ResourceCommands._textureCache) do
        if texId then
            backend.destroy_texture(texId)
        end
    end
    ResourceCommands._textureCache   = {}
    ResourceCommands._audioCache     = {}
    ResourceCommands._pendingTextures = {}
    ResourceCommands._pendingAudio   = {}
    -- Recreate placeholders (they were destroyed)
    ResourceCommands._placeholderDev     = nil
    ResourceCommands._placeholderRelease = nil
    print("[Resource] Preload cache flushed.")
end

return ResourceCommands