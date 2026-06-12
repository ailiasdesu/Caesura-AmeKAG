// test_game_state.cpp - GameState unit tests (R4.1)
#include "doctest.h"
#include "script/vm/LuaManager.h"
#include "script/state/GameState.h"
#include <string>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

TEST_CASE("GameState: push succeeds after LuaManager init") {
    LuaManager lm;
    REQUIRE(lm.init());
    lua_State* L = lm.state();

    // LuaManager::init() calls GameState::create() internally
    CHECK(GameState::push(L));
    CHECK(lua_istable(L, -1));
    lua_pop(L, 1);
}

TEST_CASE("GameState: create is idempotent") {
    LuaManager lm;
    REQUIRE(lm.init());
    lua_State* L = lm.state();

    // First call was from init(), second should be safe
    GameState::create(L);

    CHECK(GameState::push(L));
    CHECK(lua_istable(L, -1));
    lua_pop(L, 1);
}

TEST_CASE("GameState: ctx table has backlog field") {
    LuaManager lm;
    REQUIRE(lm.init());
    lua_State* L = lm.state();

    REQUIRE(GameState::push(L));
    REQUIRE(lua_istable(L, -1));

    lua_getfield(L, -1, "backlog");
    CHECK(lua_istable(L, -1));
    lua_pop(L, 1);
    lua_pop(L, 1);
}

TEST_CASE("GameState: ctx table can store custom fields") {
    LuaManager lm;
    REQUIRE(lm.init());
    lua_State* L = lm.state();

    REQUIRE(GameState::push(L));
    lua_pushstring(L, "custom_value");
    lua_setfield(L, -2, "my_field");
    lua_pop(L, 1);

    // Read back
    REQUIRE(GameState::push(L));
    lua_getfield(L, -1, "my_field");
    CHECK(lua_isstring(L, -1));
    CHECK(std::string(lua_tostring(L, -1)) == "custom_value");
    lua_pop(L, 2);
}

TEST_CASE("GameState: separate Lua states have separate ctx") {
    LuaManager lm1;
    REQUIRE(lm1.init());
    lua_State* L1 = lm1.state();

    LuaManager lm2;
    REQUIRE(lm2.init());
    lua_State* L2 = lm2.state();

    // Set field in lm1
    REQUIRE(GameState::push(L1));
    lua_pushstring(L1, "value_from_lm1");
    lua_setfield(L1, -2, "custom_key");
    lua_pop(L1, 1);

    // Check lm2 doesn't see it
    REQUIRE(GameState::push(L2));
    lua_getfield(L2, -1, "custom_key");
    CHECK(lua_isnil(L2, -1));
    lua_pop(L2, 2);
}
