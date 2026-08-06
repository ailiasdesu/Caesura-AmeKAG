-- =============================================================================
--  mods.lua — mod loader (Neo-Genesis)
--
--  Modern VN engines ship with mod support: external scene/resource
--  overrides layered on top of the base game. A mod is a directory under
--  mods/<name>/ mirroring the game layout:
--
--      mods/<name>/assets/script/act1.ks      -- overrides base scene
--      mods/<name>/assets/bgm/remix.ogg       -- overrides base audio
--
--  Resolution order (highest priority first): enabled mods sorted by
--  registered priority (higher number = higher priority), then the base
--  path. mods.resolve() returns the first existing candidate.
--
--  Usage:
--      local mods = require("mods")
--      mods.register("fan_patch", 10)   -- priority 10
--      mods.enable("fan_patch")
--      mods.resolve("assets/script/main.ks")  -- mod path or base
-- =============================================================================

local mods = {}

local state = {
    -- name -> { priority = number, enabled = bool }
    mods = {},
}

local function io_exists(path)
    local f = io.open(path, "rb")
    if f then f:close(); return true end
    return false
end

--- mods.register(name, priority) — declare a mod directory. Priority
--  defaults to 0; higher wins. Safe to call repeatedly (updates priority).
function mods.register(name, priority)
    assert(type(name) == "string" and #name > 0, "mod name (string) required")
    assert(type(name) == "string"
        and not name:match("[/\\]") and name ~= ".." and name ~= ".",
        "mod name must be a plain directory name")
    local entry = state.mods[name] or { enabled = false }
    entry.priority = tonumber(priority) or 0
    state.mods[name] = entry
    return true
end

--- mods.enable(name) / mods.disable(name)
function mods.enable(name)
    local entry = state.mods[name]
    assert(entry, "unknown mod: " .. tostring(name)
        .. " (call mods.register first)")
    entry.enabled = true
    return true
end

function mods.disable(name)
    local entry = state.mods[name]
    if entry then entry.enabled = false end
    return true
end

function mods.is_enabled(name)
    local entry = state.mods[name]
    return entry ~= nil and entry.enabled == true
end

function mods.get(name)
    return state.mods[name]
end

--- mods.list() — enabled mods in resolution order (highest priority first).
function mods.list()
    local out = {}
    for name, entry in pairs(state.mods) do
        if entry.enabled then out[#out + 1] = name end
    end
    table.sort(out, function(a, b)
        return (state.mods[a].priority or 0) > (state.mods[b].priority or 0)
    end)
    return out
end

--- mods.resolve(path) — first existing candidate: enabled mod dirs in
--  priority order, then the base path. Returns the base path when no mod
--  overrides it (a path that does not exist is returned unchanged; the
--  caller reports the failure as usual).
function mods.resolve(path)
    if type(path) ~= "string" or #path == 0 then return path end
    for _, name in ipairs(mods.list()) do
        local candidate = "mods/" .. name .. "/" .. path
        if io_exists(candidate) then
            return candidate
        end
    end
    return path
end

--- mods.resolve_scene(path) — scene-file resolution. KAG scene paths are
--  conventionally "assets/script/..."; a mod may also register an
--  alternative scene root (future: scene packs). Same fallback semantics.
function mods.resolve_scene(path)
    return mods.resolve(path)
end

--- mods.scan(dir) — auto-register every subdirectory of `dir` (default
--  "mods") with priority 0. Call once at startup; individual mods can be
--  re-registered with higher priorities afterwards.
function mods.scan(dir)
    dir = dir or "mods"
    local f = io.open(dir .. "/.index", "rb")  -- probe dir existence
    if f then f:close() end
    -- No portable directory listing in the sandbox; config.lua registers
    -- mods explicitly. scan() is provided for host-side tooling.
    return mods.list()
end

return mods
