// test_script_bindings.cpp - script bindings unit tests (R4.1)
#include "doctest.h"
#include "script/vm/LuaManager.h"
#include "script/bindings/KAGBinding.h"
#include "script/bindings/RenderBinding.h"
#include "script/bindings/VFXBinding.h"
#include "script/bindings/DebugBinding.h"
#include "script/bindings/DevCoreBinding.h"
#include "script/bindings/UnifiedBinding.h"
#include "di/BackendRegistry.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

static LuaManager* initBindingLua() {
    auto* lm = new LuaManager();
    if (!lm->init()) { delete lm; return nullptr; }
    lua_State* L = lm->state();
    BackendRegistry::instance().registerNullBackends();
    registerKAGBinding(L);
    registerRenderBinding(L);
    registerVFXBinding(L);
    registerDebugBinding(L);
    registerDevCoreBinding(L);
    registerUnifiedBackendBinding(L);
    return lm;
}

TEST_CASE("Bindings: DevCore module registered as global") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "DevCore");
    CHECK(lua_istable(L, -1));
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("Bindings: DevCore.log callable") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "DevCore");
    lua_getfield(L, -1, "log");
    CHECK(lua_isfunction(L, -1));
    lua_pushstring(L, "test message");
    int ret = lua_pcall(L, 1, 0, 0);
    CHECK(ret == LUA_OK);
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("Bindings: VFX module registered") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "VFX");
    CHECK(lua_istable(L, -1));
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("Bindings: VFX particles functions exist") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "VFX");
    REQUIRE(lua_istable(L, -1));
    lua_getfield(L, -1, "particles_create_emitter");
    CHECK(lua_isfunction(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, -1, "particles_alive_count");
    CHECK(lua_isfunction(L, -1));
    lua_pop(L, 1);
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("Bindings: Render module registered") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "Render");
    CHECK(lua_istable(L, -1));
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("Bindings: KAG.play_bgm returns boolean") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "KAG");
    lua_getfield(L, -1, "play_bgm");
    lua_pushstring(L, "nonexistent.wav");
    lua_pushnumber(L, 0.5);
    int ret = lua_pcall(L, 2, 1, 0);
    CHECK(ret == LUA_OK);
    CHECK(lua_isboolean(L, -1));
    CHECK_FALSE(lua_toboolean(L, -1)); // returns false (no audio backend)
    lua_pop(L, 2);
    delete lm;
}

TEST_CASE("Bindings: KAG.stop_bgm does not crash") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "KAG");
    lua_getfield(L, -1, "stop_bgm");
    int ret = lua_pcall(L, 0, 0, 0);
    CHECK(ret == LUA_OK);
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("Bindings: Debug module functions exist") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "Debug");
    REQUIRE(lua_istable(L, -1));
    lua_getfield(L, -1, "get_last_error");
    CHECK(lua_isfunction(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, -1, "log");
    CHECK(lua_isfunction(L, -1));
    lua_pop(L, 1);
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("Bindings: KAG global table must have expected APIs") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "KAG");
    REQUIRE(lua_istable(L, -1));
    lua_getfield(L, -1, "play_bgm");
    CHECK(lua_isfunction(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, -1, "play_se");
    CHECK(lua_isfunction(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, -1, "render_text");
    CHECK(lua_isfunction(L, -1));
    lua_pop(L, 1);
    lua_pop(L, 1);
    delete lm;
}
