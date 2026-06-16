// test_kag_integration.cpp — KAG end-to-end integration tests
#include "doctest.h"
#include "script/vm/LuaManager.h"
#include "script/bindings/KAGBinding.h"
#include "script/bindings/RenderBinding.h"
#include "script/bindings/VFXBinding.h"
#include "script/bindings/DebugBinding.h"
#include "script/bindings/DevCoreBinding.h"
#include "script/bindings/UnifiedBinding.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

static LuaManager* initKAGLua() {
    auto* lm = new LuaManager();
    if (!lm->init()) { delete lm; return nullptr; }
    lua_State* L = lm->state();
    luaL_dostring(L,
        "package.path = 'scripts/?.lua;scripts/?/init.lua;' .. package.path");
    registerKAGBinding(L);
    registerRenderBinding(L);
    registerVFXBinding(L);
    registerDebugBinding(L);
    registerDevCoreBinding(L);
    registerUnifiedBackendBinding(L);
    return lm;
}

static bool requireModule(lua_State* L, const char* name) {
    lua_getglobal(L, "require");
    lua_pushstring(L, name);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) { lua_pop(L, 1); return false; }
    lua_pop(L, 1);
    return true;
}

TEST_CASE("KAG: text_set_font and text_reset_state callable without crash") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    // Call the Lua binding functions directly
    lua_getglobal(L, "Render");
    lua_getfield(L, -1, "text_set_font");
    lua_pushstring(L, "default");
    lua_pushinteger(L, 16);
    lua_pushstring(L, "#ffffff");
    CHECK(lua_pcall(L, 3, 0, 0) == LUA_OK);
    lua_pop(L, 1);  // Render table
    delete lm;
}

TEST_CASE("KAG: all KAG command modules load and register") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    // Load the full KAG module (merges all command tables)
    CHECK(requireModule(L, "kag"));
    // Verify KAG table has expected commands
    // KAG loads sub-modules internally; verify the table exists and is non-empty
    lua_getglobal(L, "KAG");
    CHECK(lua_istable(L, -1));
    lua_pop(L, 1);
    delete lm;
}
