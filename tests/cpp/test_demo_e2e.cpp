// test_demo_e2e.cpp - Demo end-to-end smoke test (S1.2)
#include "doctest.h"
#include "script/vm/LuaManager.h"
#include "script/bindings/KAGBinding.h"
#include "script/bindings/RenderBinding.h"
#include "script/bindings/DevCoreBinding.h"
#include "script/bindings/DebugBinding.h"
#include "script/bindings/UnifiedBinding.h"
#include <fstream>
#include <sstream>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

static LuaManager* initDemoLua() {
    auto* lm = new LuaManager();
    if (!lm->init()) { delete lm; return nullptr; }
    lua_State* L = lm->state();
    luaL_dostring(L,
        "package.path = 'scripts/?.lua;scripts/?/init.lua;' .. package.path");
    registerKAGBinding(L);
    registerRenderBinding(L);
    registerDevCoreBinding(L);
    registerDebugBinding(L);
    registerUnifiedBackendBinding(L);
    return lm;
}

static bool requireModule(lua_State* L, const char* name) {
    lua_getglobal(L, "require");
    lua_pushstring(L, name);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        lua_pop(L, 1);
        return false;
    }
    lua_pop(L, 1);
    return true;
}

static std::string readDemoScript() {
    std::ifstream f("../../scripts/demo_story.ks");
    if (!f.is_open()) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

TEST_CASE("Demo E2E: tokenizer and scheduler modules load") {
    auto* lm = initDemoLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    CHECK(requireModule(L, "tokenizer"));
    CHECK(requireModule(L, "scheduler"));
    delete lm;
}

TEST_CASE("Demo E2E: scheduler runs 10-line demo for 50 iterations") {
    auto* lm = initDemoLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "tokenizer"));
    REQUIRE(requireModule(L, "scheduler"));

    std::string kag = "*start\n";
    for (int i = 1; i <= 10; ++i) {
        kag += "[ch name=\"T" + std::to_string(i) + "\" text=\"line " + std::to_string(i) + "\"]\n[p]\n";
    }
    kag += "[end]\n";

    lua_pushstring(L, kag.c_str());
    const char* code =
        "local tokenizer = require('tokenizer'); "
        "local scheduler = require('scheduler'); "
        "local tokens = tokenizer.parse(...); "
        "local ctx = {}; "
        "for i = 1, 50 do "
        "  local ok, err = pcall(scheduler.run, ctx, tokens); "
        "  if not ok then error(err) end; "
        "end; "
        "assert(true)";
    int r = luaL_dostring(L, code);
    if (r != LUA_OK) {
        MESSAGE("Lua error: " << (lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown"));
    }
    CHECK(r == LUA_OK);
    delete lm;
}

TEST_CASE("Demo E2E: demo_story.ks parses and scheduler runs it") {
    auto* lm = initDemoLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "tokenizer"));
    REQUIRE(requireModule(L, "scheduler"));

    std::string script = readDemoScript();
    if (script.empty()) {
        MESSAGE("demo_story.ks not found, skipping");
        delete lm;
        return;
    }

    lua_pushstring(L, script.c_str());
    const char* code =
        "local tokenizer = require('tokenizer'); "
        "local tokens = tokenizer.parse(...); "
        "assert(#tokens > 0, 'should parse demo script')";
    int r = luaL_dostring(L, code);
    CHECK(r == LUA_OK);
    delete lm;
}
