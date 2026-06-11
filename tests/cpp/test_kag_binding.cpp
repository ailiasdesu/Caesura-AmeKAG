#include "doctest.h"
#include "entry/Engine.h"
#include "script/vm/LuaManager.h"
#include "script/bindings/KAGBinding.h"
#include "Scripting/RenderBinding.h"
#include "Scripting/VFXBinding.h"
#include "Scripting/DebugBinding.h"
#include "Scripting/DevCoreBinding.h"
#include "Scripting/UnifiedBinding.h"
#include <thread>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

static LuaManager* initLuaWithBindings() {
    auto* lm = new LuaManager();
    if (!lm->init()) {
        delete lm;
        return nullptr;
    }
    lua_State* L = lm->state();
    registerKAGBinding(L);
    registerRenderBinding(L);
    registerVFXBinding(L);
    registerDebugBinding(L);
    registerDevCoreBinding(L);
    registerUnifiedBackendBinding(L);
    return lm;
}

TEST_CASE("KAG global table exists after registerKAGBinding") {
    auto* lm = initLuaWithBindings();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "KAG");
    CHECK(lua_istable(L, -1) == 1);
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("KAG.play_bgm is a function") {
    auto* lm = initLuaWithBindings();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "KAG");
    lua_getfield(L, -1, "play_bgm");
    CHECK(lua_isfunction(L, -1) == 1);
    lua_pop(L, 2);
    delete lm;
}

TEST_CASE("KAG.play_bgm is a function") {
    auto* lm = initLuaWithBindings();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "KAG");
    lua_getfield(L, -1, "play_bgm");
    CHECK(lua_isfunction(L, -1) == 1);
    lua_pop(L, 2);
    delete lm;
}

TEST_CASE("KAG.render_text is a function") {
    auto* lm = initLuaWithBindings();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "KAG");
    lua_getfield(L, -1, "render_text");
    CHECK(lua_isfunction(L, -1) == 1);
    lua_pop(L, 2);
    delete lm;
}

TEST_CASE("Render global table exists") {
    auto* lm = initLuaWithBindings();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "Render");
    CHECK(lua_istable(L, -1) == 1);
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("VFX global table exists") {
    auto* lm = initLuaWithBindings();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "VFX");
    CHECK(lua_istable(L, -1) == 1);
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("Debug global table exists") {
    auto* lm = initLuaWithBindings();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "Debug");
    CHECK(lua_istable(L, -1) == 1);
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("DevCore global table exists") {
    auto* lm = initLuaWithBindings();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "DevCore");
    CHECK(lua_istable(L, -1) == 1);
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("registerUnifiedBackendBinding no-crash") {
    auto* lm = initLuaWithBindings();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    registerUnifiedBackendBinding(L);
    delete lm;
}


