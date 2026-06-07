#include "doctest.h"
#include "Scripting/LuaManager.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

TEST_CASE("LuaManager::init creates valid state") {
    LuaManager lm;
    CHECK(lm.init());
    CHECK(lm.state() != nullptr);
}

TEST_CASE("LuaManager::shutdown idempotent") {
    LuaManager lm;
    lm.init();
    lm.shutdown();
    lm.shutdown();
    CHECK(lm.state() == nullptr);
}

TEST_CASE("LuaManager::lockdownScriptEnv removes dangerous globals") {
    LuaManager lm;
    lm.init();
    lm.lockdownScriptEnv();
    lua_State* L = lm.state();
    REQUIRE(L != nullptr);

    // loadfile should be removed
    lua_getglobal(L, "loadfile");
    CHECK(lua_isnil(L, -1));
    lua_pop(L, 1);

    // dofile should be removed
    lua_getglobal(L, "dofile");
    CHECK(lua_isnil(L, -1));
    lua_pop(L, 1);

    // debug should be restricted (read-only subset)
    lua_getglobal(L, "debug");
    CHECK(lua_istable(L, -1));
    lua_pop(L, 1);

    // require should be overridden (safe wrapper)
    lua_getglobal(L, "require");
    CHECK(lua_isfunction(L, -1));
    lua_pop(L, 1);
}

TEST_CASE("LuaManager::loadScript fails on nonexistent file") {
    LuaManager lm;
    lm.init();
    CHECK_FALSE(lm.loadScript("nonexistent_script.lua"));
}

TEST_CASE("LuaManager::double init is safe") {
    LuaManager lm;
    CHECK(lm.init());
    CHECK(lm.init());  // second call
    CHECK(lm.state() != nullptr);
}
