#include "doctest.h"
#include "script/vm/LuaManager.h"

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

// =============================================================================
// Expanded: instruction budget, update, resumeKAG
// =============================================================================

TEST_CASE("LuaManager::instruction budget set/get round-trip") {
    LuaManager lm;
    lm.setInstructionBudget(200000);
    CHECK(lm.getInstructionBudget() == 200000);
    lm.resetInstructionBudget();
    CHECK_FALSE(lm.isInstructionBudgetExceeded());
}

TEST_CASE("LuaManager::instruction budget interrupts infinite loop") {
    LuaManager lm;

    // Set very low budget (100 instructions) and run an infinite loop
    // The instruction hook fires on LuaManager::instance(), not on locals.
    auto& mgr = LuaManager::instance();
    REQUIRE(mgr.init());
    lua_State* L = mgr.state();
    REQUIRE(L != nullptr);

    // Save budget to restore after test
    int oldBudget = mgr.getInstructionBudget();
    mgr.setInstructionBudget(100);
    // With budget=100 and hook firing every 1000 instrs, the loop will
    // be interrupted after multiple hook invocations. Verify it doesn't hang.
    int result = luaL_dostring(L, "while true do end");
    // The loop should be interrupted (not hang forever)
    CHECK(result != LUA_OK);
    mgr.resetInstructionBudget();
    mgr.setInstructionBudget(oldBudget);  // Restore for other tests
    CHECK_FALSE(mgr.isInstructionBudgetExceeded());
}

TEST_CASE("LuaManager::update does not crash") {
    LuaManager lm;
    lm.init();
    lm.update(0.016f);
    lm.update(0.0f);
}

TEST_CASE("LuaManager::resumeKAGCoroutine does not crash without coroutine") {
    LuaManager lm;
    lm.init();
    // No KAG coroutine running — should be a safe no-op
    lm.resumeKAGCoroutine();
}

