 extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include "LuaManager.h"
#include "KAGBinding.h"
#include "RenderBinding.h"
#include "DevCoreBinding.h"
#include "DebugBinding.h"
#include "UnifiedBinding.h"
#include "../System/SaveBinding.h"
#include "GameState.h"
#include "../Core/BackendRegistry.h"
#include <cstdio>

namespace Caesura {

bool LuaManager::init() {
    if (m_initialized) return true;

    m_L = luaL_newstate();
    if (!m_L) {
        fprintf(stderr, "[Lua] Failed to create Lua state.\n");
        return false;
    }

    luaL_openlibs(m_L);

    // Create the global KAG context table in Lua registry before modules load
    GameState::create(m_L);

    registerModules();

    m_initialized = true;
    printf("[Lua] VM initialized.\n");
    return true;
}


void LuaManager::lockdownScriptEnv() {
    // Remove dangerous top-level functions
    lua_pushnil(m_L); lua_setglobal(m_L, "loadfile");
    lua_pushnil(m_L); lua_setglobal(m_L, "dofile");
    
    // Replace debug library with read-only safe subset
    // Kept:    getinfo, traceback, getlocal, getupvalue, getuservalue, getmetatable
    // Removed: setupvalue, sethook, setlocal, setmetatable, getregistry, upvaluejoin, setuservalue
    luaL_dostring(m_L,
        "if _G.debug then\n"
        "  local raw_debug = _G.debug\n"
        "  _G.debug = {\n"
        "    getinfo     = raw_debug.getinfo,\n"
        "    traceback   = raw_debug.traceback,\n"
        "    getlocal    = raw_debug.getlocal,\n"
        "    getupvalue  = raw_debug.getupvalue,\n"
        "    getuservalue= raw_debug.getuservalue,\n"
        "    getmetatable= raw_debug.getmetatable,\n"
        "  }\n"
        "end\n"
    );
    
    
    // Clear package.searchers (Lua 5.2+) / package.loaders (Lua 5.1) 
    // to prevent filesystem search via require
    lua_getglobal(m_L, "package");
    if (lua_istable(m_L, -1)) {
        lua_pushnil(m_L); lua_setfield(m_L, -2, "loadlib");
        lua_pushnil(m_L); lua_setfield(m_L, -2, "searchpath");
        // Clear searchers/loaders table
        lua_getfield(m_L, -1, "searchers");
        if (!lua_istable(m_L, -1)) {
            lua_pop(m_L, 1);
            lua_getfield(m_L, -1, "loaders");
        }
        if (lua_istable(m_L, -1)) {
            // Empty the searchers table so require can only return cached modules
            lua_newtable(m_L);
            lua_setfield(m_L, -2, "searchers");
        }
        lua_pop(m_L, 1);
    }
    lua_pop(m_L, 1);
    
    // Replace require with safe wrapper: only returns from package.loaded (already-loaded modules)
    // Safe: all modules loaded at startup; no new filesystem access possible
    luaL_dostring(m_L,
        "_G.require = function(name)\n"
        "  local loaded = package.loaded[name]\n"
        "  if loaded ~= nil then return loaded end\n"
        "  error('Sandbox: module \"' .. name .. '\" not preloaded. Add to config.lua.', 2)\n"
        "end"
    );
    
    // Replace os.execute + os.remove with no-op stubs (directory ops handled by C API)
    // Keep os.clock, os.date, os.time, os.difftime for legitimate use
    if (m_L) {
        luaL_dostring(m_L,
            "if _G.os then\n"
            "  _G.os.execute = function(cmd) return -1 end\n"
            "  _G.os.remove = function(path) return nil, 'sandboxed' end\n"
            "  _G.os.rename = function() return nil, 'sandboxed' end\n"
            "  _G.os.exit = function() error('os.exit disabled', 2) end\n"
            "end"
        );
    }
    
    // Replace io.open with no-op (prevents file write/read)
    luaL_dostring(m_L,
        "if _G.io then\n"
        "  _G.io.open = function(fn, mode)\n"
        "    if mode == 'r' then return nil, 'io.open read disabled' end\n"
        "    return nil, 'io.open write disabled'\n"
        "  end\n"
        "  _G.io.popen = function() return nil, 'io.popen disabled' end\n"
        "end"
    );
    
    printf("[Lua] Script environment locked down (safe require + stubs).\n");
}
void LuaManager::shutdown() {
    if (m_L) {
        lua_close(m_L);
        m_L = nullptr;
    }
    m_initialized = false;
    printf("[Lua] VM shut down.\n");
}

void LuaManager::update(float /*deltaTime*/) {
    // Event-driven; no polling needed
}

void LuaManager::registerModules() {

    // -- Security: sandboxing moved to lockdownScriptEnv() (called after scripts load) --

    BackendRegistry::registerEngineBindings(m_L);
    registerKAGBinding(m_L);
    registerRenderBinding(m_L);
    registerDevCoreBinding(m_L);
    registerDebugBinding(m_L);
    registerUnifiedBackendBinding(m_L);
    registerSaveBinding(m_L);

    printf("[Lua] Engine (backend selection) module registered.\n");
    printf("[Lua] KAG module registered (32 APIs, via BackendRegistry).\n");
    printf("[Lua] Render module registered (via BackendRegistry).\n");
    printf("[Lua] DevCore module registered (via BackendRegistry).\n");
    printf("[Lua] Debug module registered (8 APIs).\n");
}


bool LuaManager::loadScript(const char* path) {
    if (!m_L || !path) return false;
    printf("[Lua] Loading script: %s\n", path);
    if (luaL_dofile(m_L, path) != LUA_OK) {
        fprintf(stderr, "[Lua] Error loading %s: %s\n", path, lua_tostring(m_L, -1));
        lua_pop(m_L, 1);
        return false;
    }
    return true;
}

void LuaManager::resumeKAGCoroutine() {
    // Reserved for future use
}
} // namespace Caesura
