 extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include "../vm/LuaManager.h"
#include "../bindings/KAGBinding.h"
#include "../bindings/RenderBinding.h"
#include "../bindings/DevCoreBinding.h"
#include "../bindings/DebugBinding.h"
// #include "UnifiedBinding.h"  // deprecated, BackendFactory handles _CAESURA_BACKEND
#include "../bindings/VFXBinding.h"
#include "../storage/SaveBinding.h"
#include "../bindings/SteamBinding.h"
#include "../state/GameState.h"
#include "../di/BackendRegistry.h"
#include "../di/thread/ThreadAssert.h"
#include <cstdio>

namespace Caesura {

LuaManager& LuaManager::instance() {
    static LuaManager mgr;
    return mgr;
}


// ===========================================================================
//  Track 3: Instruction-count hook for CPU budget enforcement
// ===========================================================================

void LuaManager::instructionHook(lua_State* L, lua_Debug* /*ar*/) {
    auto& mgr = LuaManager::instance();
    mgr.m_instructionCount++;
    if (mgr.m_instructionCount > mgr.m_instructionBudget) {
        mgr.m_budgetExceeded = true;
        // Force a Lua error to unwind the stack
        luaL_error(L, "Sandbox: instruction budget exceeded (%d instructions)",
                   mgr.m_instructionBudget);
    }
}

bool LuaManager::init() {
    if (m_initialized) return true;
    CAESURA_ASSERT_MAIN_THREAD();

    m_L = luaL_newstate();
    if (!m_L) {
        fprintf(stderr, "[Lua] Failed to create Lua state.\n");
        return false;
    }

    luaL_openlibs(m_L);

    // Create the global KAG context table in Lua registry before modules load
    GameState::create(m_L);

    registerModules();

    // Track 3: Instruction-count hook for CPU budget (every 1000 instructions)
    lua_sethook(m_L, instructionHook, LUA_MASKCOUNT, 1000);

    m_initialized = true;
    printf("[Lua] VM initialized.\n");
    return true;
}


void LuaManager::lockdownScriptEnv() {
    // Load sandbox rules from Lua module (human-readable, AI-inspectable)
    // All dangerous globals, module restrictions, and I/O stubs defined there.
    if (luaL_dofile(m_L, "scripts/sandbox.lua") != LUA_OK) {
        fprintf(stderr, "[Lua] Failed to load sandbox.lua: %s\n",
                lua_tostring(m_L, -1));
        lua_pop(m_L, 1);
    } else {
        // sandbox.lua returns the Sandbox module table; pop it to keep the stack clean
        lua_pop(m_L, 1);
        printf("[Lua] Script environment locked down (sandbox.lua).\n");
    }
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
    CAESURA_ASSERT_MAIN_THREAD();
    // Event-driven; no polling needed
}

void LuaManager::registerModules() {
    CAESURA_ASSERT_MAIN_THREAD();

    // -- Security: sandboxing moved to lockdownScriptEnv() (called after scripts load) --

    BackendRegistry::registerEngineBindings(m_L);
    registerKAGBinding(m_L);
    registerRenderBinding(m_L);
    registerDevCoreBinding(m_L);
    registerDebugBinding(m_L);
    // registerUnifiedBackendBinding(m_L);  // deprecated, replaced by BackendFactory
    registerSaveBinding(m_L);
    registerVFXBinding(m_L);

    printf("[Lua] Engine (backend selection) module registered.\n");
    printf("[Lua] KAG module registered (32 APIs, via BackendRegistry).\n");
    printf("[Lua] Render module registered (via BackendRegistry).\n");
    printf("[Lua] DevCore module registered (via BackendRegistry).\n");
    printf("[Lua] Debug module registered (8 APIs).\n");
}


bool LuaManager::loadScript(const char* path) {
    CAESURA_ASSERT_MAIN_THREAD();
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
    CAESURA_ASSERT_MAIN_THREAD();
    // Reserved for future use
}
} // namespace Caesura

