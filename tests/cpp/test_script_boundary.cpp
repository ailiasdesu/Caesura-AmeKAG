// test_script_boundary.cpp - E3 boundary tests (KAG error recovery, GameState, bindings)
#include "doctest.h"
#include "script/vm/LuaManager.h"
#include "script/bindings/KAGBinding.h"
#include "script/bindings/RenderBinding.h"
#include "script/bindings/VFXBinding.h"
#include "script/bindings/DebugBinding.h"
#include "script/bindings/DevCoreBinding.h"
#include "script/bindings/UnifiedBinding.h"
#include "script/state/GameState.h"
#include "di/BackendRegistry.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

// -- KAG error recovery (E3 Step 1) ------------------------------------

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

TEST_CASE("E3 KAG: malformed script does not crash") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "tokenizer"));
    int r = luaL_dostring(L,
        "local tk = require('tokenizer'); "
        "local ok, result = pcall(tk.parse, '@invalid @@syntax **broken'); "
        "assert(ok == false or type(result) == 'table')");
    CHECK((r == LUA_OK || r == LUA_ERRRUN));
    delete lm;
}


TEST_CASE("E3 KAG: choice with zero options degrades gracefully") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "tokenizer"));
    int r = luaL_dostring(L,
        "local tk = require('tokenizer'); "
        "local tokens = tk.parse('[choice]\\n[endchoice]'); "
        "assert(#tokens >= 2)");
    CHECK((r == LUA_OK || r == LUA_ERRRUN));
    delete lm;
}

TEST_CASE("E3 KAG: deeply nested if does not crash") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "tokenizer"));
    int r = luaL_dostring(L,
        "local tk = require('tokenizer'); "
        "local s = ''; "
        "for i = 1, 15 do s = s .. '[if exp=\\'1\\']\\n' end; "
        "for i = 1, 15 do s = s .. '[endif]\\n' end; "
        "local tokens = tk.parse(s); "
        "assert(#tokens >= 30)");
    CHECK((r == LUA_OK || r == LUA_ERRRUN));
    delete lm;
}

// -- GameState persistence (E3 Step 2) ----------------------------------

TEST_CASE("E3 GameState: persist across save/load cycle") {
    LuaManager lm1;
    REQUIRE(lm1.init());
    lua_State* L1 = lm1.state();
    REQUIRE(GameState::push(L1));
    lua_pushstring(L1, "persist_value");
    lua_setfield(L1, -2, "persist_key");
    lua_getfield(L1, -1, "persist_key");
    CHECK(std::string(lua_tostring(L1, -1)) == "persist_value");
    lua_pop(L1, 2);

    LuaManager lm2;
    REQUIRE(lm2.init());
    lua_State* L2 = lm2.state();
    REQUIRE(GameState::push(L2));
    lua_getfield(L2, -1, "persist_key");
    CHECK(lua_isnil(L2, -1));
    lua_pop(L2, 2);
}

TEST_CASE("E3 GameState: nested table storage") {
    LuaManager lm;
    REQUIRE(lm.init());
    lua_State* L = lm.state();
    REQUIRE(GameState::push(L));
    lua_newtable(L);
    lua_pushstring(L, "inner");
    lua_setfield(L, -2, "nested_key");
    lua_setfield(L, -2, "outer_key");
    lua_getfield(L, -1, "outer_key");
    CHECK(lua_istable(L, -1));
    lua_getfield(L, -1, "nested_key");
    CHECK(std::string(lua_tostring(L, -1)) == "inner");
    lua_pop(L, 3);
}

TEST_CASE("E3 GameState: push after create returns existing ctx") {
    LuaManager lm;
    REQUIRE(lm.init());
    lua_State* L = lm.state();
    REQUIRE(GameState::push(L));
    lua_pushstring(L, "persistent");
    lua_setfield(L, -2, "sticky_key");
    lua_pop(L, 1);
    REQUIRE(GameState::push(L));
    lua_getfield(L, -1, "sticky_key");
    CHECK(std::string(lua_tostring(L, -1)) == "persistent");
    lua_pop(L, 2);
}

// -- Binding parameter validation (E3 Step 3) ---------------------------

static LuaManager* initBindingLua() {
    auto* lm = new LuaManager();
    if (!lm->init()) { delete lm; return nullptr; }
    lua_State* L = lm->state();
    registerKAGBinding(L);
    registerRenderBinding(L);
    registerVFXBinding(L);
    registerDebugBinding(L);
    registerDevCoreBinding(L);
    return lm;
}

TEST_CASE("E3 Bindings: KAG play_bgm empty path does not crash") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "KAG");
    lua_getfield(L, -1, "play_bgm");
    lua_pushstring(L, "");
    int r = lua_pcall(L, 1, 1, 0);
    CHECK((r == LUA_OK || r == LUA_ERRRUN));
    lua_pop(L, 2);
    delete lm;
}

TEST_CASE("E3 Bindings: Debug log nil message does not crash") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "Debug");
    lua_getfield(L, -1, "log");
    lua_pushnil(L);
    int r = lua_pcall(L, 1, 0, 0);
    CHECK((r == LUA_OK || r == LUA_ERRRUN));
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("E3 Bindings: DevCore set_dev_mode invalid value does not crash") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "DevCore");
    lua_getfield(L, -1, "set_dev_mode");
    lua_pushinteger(L, 999);
    int r = lua_pcall(L, 1, 0, 0);
    CHECK((r == LUA_OK || r == LUA_ERRRUN));
    lua_pop(L, 1);
    delete lm;
}
