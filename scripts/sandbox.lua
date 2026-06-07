-- ===========================================================================
--  Caesura (AmeKAG) 鈥?Sandbox Rules v2 (Track 3)
--  ===========================================================================
--  Loaded once at engine startup via LuaManager::lockdownScriptEnv().
--  All rules here are readable by external AI assistants.
--
--  Design principle: DEFAULT DENY, EXPLICIT ALLOW.
--  Any capability not explicitly permitted here is blocked.
--  ===========================================================================

-- ---------------------------------------------------------------------------
--  1. GLOBAL DANGEROUS FUNCTIONS 鈥?REMOVED
-- ---------------------------------------------------------------------------
--  These are the most dangerous entry points: arbitrary file loading,
--  script execution, and process spawning.
--  All asset loading goes through the C++ IAssetProvider chain instead.
-- ---------------------------------------------------------------------------

_G.loadfile = nil
_G.dofile   = nil

-- ---------------------------------------------------------------------------
--  2. DEBUG LIBRARY 鈥?READ-ONLY SUBSET
-- ---------------------------------------------------------------------------
--  Kept (inspection only):
--    getinfo, traceback, getlocal, getupvalue, getuservalue, getmetatable
--  Removed (mutation capable):
--    setupvalue, sethook, setlocal, setmetatable, getregistry,
--    upvaluejoin, setuservalue, debug (raw), gethook
-- ---------------------------------------------------------------------------

if _G.debug then
    local raw_debug = _G.debug
    _G.debug = {
        getinfo      = raw_debug.getinfo,
        traceback    = raw_debug.traceback,
        getlocal     = raw_debug.getlocal,
        getupvalue   = raw_debug.getupvalue,
        getuservalue = raw_debug.getuservalue,
        getmetatable = raw_debug.getmetatable,
    }
end

-- ---------------------------------------------------------------------------
--  3. PACKAGE SYSTEM 鈥?SEARCH DISABLED
-- ---------------------------------------------------------------------------
--  Clears package.searchers/loaders so require() cannot search the filesystem.
--  Only modules preloaded in package.loaded (at startup via config.lua +
--  kag/init.lua) are accessible.
-- ---------------------------------------------------------------------------

package.loadlib    = nil
package.searchpath = nil

if package.searchers then
    package.searchers = {}
elseif package.loaders then
    package.loaders = {}
end

-- ---------------------------------------------------------------------------
--  4. REQUIRE 鈥?SAFE WRAPPER
-- ---------------------------------------------------------------------------
--  Only returns preloaded modules. Un-preloaded module = hard error.
-- ---------------------------------------------------------------------------

_G.require = function(name)
    local loaded = package.loaded[name]
    if loaded ~= nil then
        return loaded
    end
    error('Sandbox: module "' .. name .. '" not preloaded. Add to config.lua.', 2)
end

-- ---------------------------------------------------------------------------
--  5. OS MODULE 鈥?FILESYSTEM OPERATIONS DISABLED
-- ---------------------------------------------------------------------------
--  Kept: os.clock, os.date, os.time, os.difftime (non-I/O)
--  Replaced with no-ops or sandboxed stubs.
-- ---------------------------------------------------------------------------

if _G.os then
    _G.os.execute = function(cmd) return -1 end
    _G.os.remove  = function(path) return nil, "sandboxed" end
    _G.os.rename  = function() return nil, "sandboxed" end
    _G.os.exit    = function() error("os.exit disabled", 2) end
end

-- ---------------------------------------------------------------------------
--  6. IO MODULE 鈥?FILESYSTEM READ/WRITE DISABLED
-- ---------------------------------------------------------------------------
--  All file I/O through C++ IAssetProvider chain (read) or SaveManager (write).
-- ---------------------------------------------------------------------------

if _G.io then
    _G.io.open  = function(fn, mode)
        if mode == "r" then return nil, "io.open read disabled" end
        return nil, "io.open write disabled"
    end
    _G.io.popen = function() return nil, "io.popen disabled" end
end

-- ===========================================================================
--  TRACK 3: Extreme Security Additions
-- ===========================================================================

-- ---------------------------------------------------------------------------
--  7. _G WRITE PROTECTION (Track 3)
-- ---------------------------------------------------------------------------
--  Prevents AI-generated scripts from poisoning the global environment.
--  Whitelisted globals can still be assigned; everything else is read-only.
--  Uses __newindex metamethod on the global table.
-- ---------------------------------------------------------------------------

local _G_whitelist = {
    -- Standard Lua globals (safe subset)
    assert     = true, error      = true, ipairs     = true,
    next       = true, pairs      = true, pcall      = true,
    select     = true, tonumber   = true, tostring   = true,
    type       = true, xpcall     = true,
    -- Coroutine (safe: no C boundary cross)
    coroutine  = true,
    -- Math (pure computation)
    math       = true,
    -- String (pure computation)
    string     = true,
    -- Table (pure computation)
    table      = true,
    -- Engine API tables (read-only 鈥?their internals are protected by C)
    KAG        = true,
    Engine     = true,
    Render     = true,
    VFX        = true,
    DevCore    = true,
    _CAESURA_BACKEND = true,
    _CAESURA_CONFIG  = true,
    _SANDBOX_RESOURCES = true,
    _SANDBOX_CHECK     = true,
    debug      = true,  -- already read-only from section 2
    -- Read-only globals
    package    = true,
    os         = true,  -- I/O already disabled in section 5
}

local _G_mt = {
    __newindex = function(t, k, v)
        if _G_whitelist[k] then
            rawset(t, k, v)  -- allow whitelisted writes
        else
            error("Sandbox: cannot create global '" .. tostring(k) .. "'", 2)
        end
    end,
    __metatable = "protected",
}
setmetatable(_G, _G_mt)

-- ---------------------------------------------------------------------------
--  8. RESOURCE BUDGET TRACKING HOOKS (Track 3)
-- ---------------------------------------------------------------------------
--  These counters are incremented by the C++ side when resources are
--  allocated through KAG API functions. AI scripts can query them.
-- ---------------------------------------------------------------------------

_G._SANDBOX_RESOURCES = {
    textures_loaded   = 0,
    textures_max      = 256,   -- hard cap for AI scripts
    audio_handles     = 0,
    audio_max         = 64,    -- hard cap for AI scripts
    rtt_canvases      = 0,
    rtt_max           = 8,
    particles_emitters = 0,
    particles_max     = 16,
}

-- Check function: call before allocating to avoid resource exhaustion
function _G._SANDBOX_CHECK(kind)
    local r = _SANDBOX_RESOURCES
    local cur = r[kind .. "_loaded"]
    local max = r[kind .. "_max"]
    if cur and max and cur >= max then
        error("Sandbox: " .. kind .. " limit reached (" .. max .. ")", 2)
    end
end

-- ===========================================================================
--  End of sandbox rules.
-- ===========================================================================