-- ===========================================================================
--  Caesura (AmeKAG) — Sandbox Rules
--  ===========================================================================
--  Loaded once at engine startup via LuaManager::lockdownScriptEnv().
--  All rules here are readable by external AI assistants so they understand
--  exactly which Lua APIs are available and which are restricted.
--
--  Design principle: DEFAULT DENY, EXPLICIT ALLOW.
--  Any capability not explicitly permitted here is blocked.
--  ===========================================================================

-- ---------------------------------------------------------------------------
--  1. GLOBAL DANGEROUS FUNCTIONS — REMOVED
-- ---------------------------------------------------------------------------
--  These are the most dangerous entry points: they allow arbitrary file
--  loading and script execution from the filesystem.
--  All asset loading goes through the C++ IAssetProvider chain instead.
-- ---------------------------------------------------------------------------

_G.loadfile = nil   -- Removed: arbitrary .lua file loading
_G.dofile   = nil   -- Removed: same as loadfile + execute

-- ---------------------------------------------------------------------------
--  2. DEBUG LIBRARY — READ-ONLY SUBSET
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
--  3. PACKAGE SYSTEM — SEARCH DISABLED
-- ---------------------------------------------------------------------------
--  Clears package.searchers/loaders so require() cannot search the filesystem.
--  Only modules already preloaded in package.loaded (loaded at startup via
--  config.lua / kag/init.lua) are accessible.
-- ---------------------------------------------------------------------------

package.loadlib    = nil   -- Removed: C library loading
package.searchpath = nil   -- Removed: filesystem path search

if package.searchers then
    package.searchers = {}
elseif package.loaders then
    package.loaders = {}
end

-- ---------------------------------------------------------------------------
--  4. REQUIRE — SAFE WRAPPER
-- ---------------------------------------------------------------------------
--  Replaced with a version that only returns preloaded modules.
--  Attempting to require an un-preloaded module is a hard error.
-- ---------------------------------------------------------------------------

_G.require = function(name)
    local loaded = package.loaded[name]
    if loaded ~= nil then
        return loaded
    end
    error('Sandbox: module "' .. name .. '" not preloaded. Add to config.lua.', 2)
end

-- ---------------------------------------------------------------------------
--  5. OS MODULE — FILESYSTEM OPERATIONS DISABLED
-- ---------------------------------------------------------------------------
--  Kept: os.clock, os.date, os.time, os.difftime (legitimate non-I/O use)
--  Replaced with no-ops or sandboxed stubs:
-- ---------------------------------------------------------------------------

if _G.os then
    _G.os.execute = function(cmd) return -1 end
    _G.os.remove  = function(path) return nil, "sandboxed" end
    _G.os.rename  = function() return nil, "sandboxed" end
    _G.os.exit    = function() error("os.exit disabled", 2) end
    -- Note: os.tmpname is NOT overridden — it returns a string but doesn't
    -- create files. The io module prevents actual file creation.
end

-- ---------------------------------------------------------------------------
--  6. IO MODULE — FILESYSTEM READ/WRITE DISABLED
-- ---------------------------------------------------------------------------
--  All file I/O goes through the C++ IAssetProvider chain (read) or
--  SaveManager (write). Direct Lua filesystem access is prohibited.
-- ---------------------------------------------------------------------------

if _G.io then
    _G.io.open  = function(fn, mode)
        if mode == "r" then
            return nil, "io.open read disabled"
        end
        return nil, "io.open write disabled"
    end
    _G.io.popen = function() return nil, "io.popen disabled" end
end

-- ===========================================================================
--  End of sandbox rules.
-- ===========================================================================