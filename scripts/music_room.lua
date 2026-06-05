-- ===========================================================================
--  Caesura (AmeKAG) — music_room.lua  [P1.2]
--  Music Room: browse BGM tracks, preview playback, mark favorites.
--  Favorite state persisted in independent config (config/music_room.lua).
-- ===========================================================================

local MusicRoom = {}
local backend = require("backend")
local audio   = require("audio")
local fileutil = require("fileutil")

local trackCache = nil
local favorites = {}
local currentPreview = nil

-- [P1-2] Capture os.execute before potential sandboxing
local _os_execute = os.execute

-- ===========================================================================
-- MusicRoom.scan() -> { {id, path, name}, ... }
-- Cross-platform scan (W7).
-- ===========================================================================
function MusicRoom.scan()
    if trackCache then return trackCache end
    trackCache = {}
    local dirs = {"assets/bgm/", "assets/bgm", "data/bgm/", "data/bgm"}
    local pattern = "%.ogg$|%.mp3$|%.wav$"
    for _, dir in ipairs(dirs) do
        local files = fileutil.scan_dir(dir, pattern)
        if #files > 0 then
            for _, fname in ipairs(files) do
                local id = fname:match("^(.-)%.[^.]+$") or fname
                table.insert(trackCache, {id = id, path = dir .. "/" .. fname, name = id})
            end
            break
        end
    end
    table.sort(trackCache, function(a, b) return a.id < b.id end)
    return trackCache
end

-- ===========================================================================
-- MusicRoom.list() -> {id, name, favorited}
-- ===========================================================================
function MusicRoom.list()
    local tracks = MusicRoom.scan()
    MusicRoom._loadFavorites()
    local result = {}
    for _, t in ipairs(tracks) do
        table.insert(result, {
            id = t.id,
            name = t.name,
            favorited = favorites[t.id] == true,
            path = t.path,
        })
    end
    return result
end

-- ===========================================================================
-- MusicRoom.play(id) — preview a track (W5: fixed opts table)
-- ===========================================================================
function MusicRoom.play(id)
    MusicRoom.stop()
    local tracks = MusicRoom.scan()
    for _, t in ipairs(tracks) do
        if t.id == id then
            audio.play_bgm(t.path, { fadein = 0.5 })
            currentPreview = id
            print("[MusicRoom] Playing: " .. id)
            return true
        end
    end
    print("[MusicRoom] Track not found: " .. id)
    return false
end

-- ===========================================================================
-- MusicRoom.stop()
-- ===========================================================================
function MusicRoom.stop()
    if currentPreview then
        audio.stop_bgm(0.3)
        currentPreview = nil
    end
end

-- ===========================================================================
-- MusicRoom.favorite(id) — toggle favorite mark
-- ===========================================================================
function MusicRoom.favorite(id)
    favorites[id] = not favorites[id]
    MusicRoom._saveFavorites()
    print("[MusicRoom] " .. id .. " favorited: " .. tostring(favorites[id]))
    return favorites[id]
end

-- ===========================================================================
-- MusicRoom._loadFavorites()
-- ===========================================================================
function MusicRoom._loadFavorites()
    local ok, data = pcall(function()
        local f = io.open("config/music_room.lua", "r")
        if not f then return {} end
        local txt = f:read("*a")
        f:close()
        return load("return " .. txt)()
    end)
    if ok and type(data) == "table" then
        favorites = data
    end
end

-- ===========================================================================
-- MusicRoom._saveFavorites()
-- ===========================================================================
function MusicRoom._saveFavorites()
    pcall(function()
        -- Cross-platform mkdir (W4)
    local isWindows = (package.config:sub(1,1) == "\\")
    if isWindows then
        _os_execute('mkdir "config" 2>nul')
    else
        _os_execute('mkdir -p "config" 2>/dev/null')
    end
        local f = io.open("config/music_room.lua", "w")
        if not f then return end
        f:write("{\n")
        local first = true
        for k, v in pairs(favorites) do
            if not first then f:write(",\n") end
            f:write('  ["' .. k .. '"] = ' .. tostring(v))
            first = false
        end
        f:write("}\n")
        f:close()
    end)
end

-- ===========================================================================
-- MusicRoom.show(ctx)
-- ===========================================================================
function MusicRoom.show(ctx)
    local tracks = MusicRoom.list()
    if #tracks == 0 then
        backend.render_text("[Music Room] No tracks found.", 32, 100)
        return
    end
    backend.render_text("=== Music Room ===", 32, 50)
    for i, t in ipairs(tracks) do
        local fav = t.favorited and " <3" or ""
        local line = string.format("%2d. %s%s", i, t.name, fav)
        backend.render_text(line, 32, 50 + i * 24)
    end
    backend.render_text("Click a track to preview.", 32, 50 + (#tracks + 1) * 24)
end

return MusicRoom
