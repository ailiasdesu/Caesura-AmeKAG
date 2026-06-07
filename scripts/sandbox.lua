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
-- ---------------------------------------------------------------------------
--  9. RENDER OPERATION WHITELIST (Track 3) -- "DEFAULT DENY, EXPLICIT ALLOW"
-- ---------------------------------------------------------------------------
--  In strict mode, engine API tables (Render, DevCore) are wrapped with
--  __index proxies. Calls to non-whitelisted functions are blocked.
--  Set _SANDBOX_MODE = "dev" (or omit) to disable whitelist enforcement.
-- ---------------------------------------------------------------------------

-- Whitelist: Render module -- allowed functions for AI scripts
local RENDER_WHITELIST = {
    load_texture        = true,
    destroy_texture     = true,
    create_solid_texture = true,
    get_resolution      = true,
    set_view_name       = true,
    submit_batch        = true,
    submit_blend        = true,
    load_texture_async  = true,
    cancel_async_loads  = true,
}

-- Whitelist: DevCore module -- allowed functions for AI scripts
local DEVCORE_WHITELIST = {
    set_input_focus     = true,
    get_input_focus     = true,
    log                 = true,
    get_resolution      = true,
    get_window_size     = true,
}

-- Whitelist: Debug module -- read-only inspection only
local DEBUG_WHITELIST = {
    get_last_error      = true,
    get_error_count     = true,
    get_subsystem_stats = true,
    dump_report         = true,
    get_render_info     = true,
    get_audio_info      = true,
    get_input_info      = true,
    get_log_path        = true,
    log                 = true,
    get_stats           = true,
}

-- Build a call-proxy metatable for a module table
local function make_whitelist_proxy(original, whitelist, module_name)
    local proxy = {}
    local mt = {
        __index = function(t, k)
            local v = original[k]
            if v == nil then return nil end
            if type(v) == "function" then
                if whitelist[k] then
                    return v  -- allowed: return original function
                else
                    error("Sandbox: " .. module_name .. "." .. k .. " is blocked in strict mode", 2)
                end
            end
            return v  -- non-function fields: always readable
        end,
        __newindex = function(t, k, v)
            error("Sandbox: cannot modify " .. module_name .. " table", 2)
        end,
        __metatable = "protected",
    }
    setmetatable(proxy, mt)
    return proxy
end

-- Apply whitelist proxies in strict mode
-- Default mode is "strict" -- AI scripts get maximum protection.
-- Developers can set _SANDBOX_MODE = "dev" in config.lua for full access.
if rawget(_G, "_SANDBOX_MODE") == nil then
    rawset(_G, "_SANDBOX_MODE", "strict")
end

if _SANDBOX_MODE == "strict" then
    -- KAG: full access (safe high-level API, all ops go through BackendRegistry)
    -- VFX: full access (already quota-controlled by C++ side)
    -- Unified (_CAESURA_BACKEND): delegates through whitelisted modules, inherits restrictions

    if Render then
        _G.Render = make_whitelist_proxy(Render, RENDER_WHITELIST, "Render")
    end
    if DevCore then
        _G.DevCore = make_whitelist_proxy(DevCore, DEVCORE_WHITELIST, "DevCore")
    end
    if Debug then
        _G.Debug = make_whitelist_proxy(Debug, DEBUG_WHITELIST, "Debug")
    end

    print("[Sandbox] Strict mode: render operation whitelist active")
else
    print("[Sandbox] Dev mode: whitelist disabled, full API access")
end

-- ===========================================================================
--  End of sandbox rules.
-- ===========================================================================
