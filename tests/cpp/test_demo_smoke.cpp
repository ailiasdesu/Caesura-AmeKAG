// test_demo_smoke.cpp - demo script smoke test (T5)
#include "doctest.h"
#include "script/vm/LuaManager.h"
#include "script/bindings/KAGBinding.h"
#include "script/bindings/RenderBinding.h"
#include "script/bindings/DevCoreBinding.h"
#include "script/bindings/DebugBinding.h"

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
    return lm;
}

TEST_CASE("Demo: core KAG flow parses") {
    auto* lm = initDemoLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();

    const char* code =
        "local tk = require('tokenizer')\n"
        "local demo = '*start\\n[ch name=\"Hero\" text=\"Hello!\"]\\n[p]\\n[bg storage=\"bg.png\"]\\n[wait time=300]\\n[end]'\n"
        "local tokens = tk.parse(demo)\n"
        "assert(#tokens >= 6, 'got ' .. #tokens)\n"
        "assert(tokens[1].type == 'label')\n"
        "assert(tokens[1].name == 'start')\n";
    CHECK(luaL_dostring(L, code) == LUA_OK);
    delete lm;
}

TEST_CASE("Demo: choice buttons parse") {
    auto* lm = initDemoLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();

    const char* code =
        "local tk = require('tokenizer')\n"
        "local s = '[button text=\"A\" target=\"*a\"]\\n[button text=\"B\" target=\"*b\"]\\n[endbutton]'\n"
        "local tokens = tk.parse(s)\n"
        "local n = 0\n"
        "for _, t in ipairs(tokens) do if t.type == 'command' and t.cmd == 'button' then n = n + 1 end end\n"
        "assert(n == 2, 'expected 2 buttons')\n";
    CHECK(luaL_dostring(L, code) == LUA_OK);
    delete lm;
}

TEST_CASE("Demo: bgm + stopbgm flow") {
    auto* lm = initDemoLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();

    const char* code =
        "local tk = require('tokenizer')\n"
        "local demo = '[playbgm storage=\"bgm/title.ogg\" volume=0.8]\\n[stopbgm time=500]'\n"
        "local tokens = tk.parse(demo)\n"
        "assert(#tokens >= 2, 'got ' .. #tokens)\n"
        "assert(tokens[1].cmd == 'playbgm', 'first should be playbgm')\n"
        "assert(tokens[2].cmd == 'stopbgm', 'second should be stopbgm')\n";
    CHECK(luaL_dostring(L, code) == LUA_OK);
    delete lm;
}

TEST_CASE("Demo: advance 30 lines without crash") {
    auto* lm = initDemoLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();

    const char* code =
        "local tk = require('tokenizer')\n"
        "local lines = {}\n"
        "for i = 1, 30 do lines[#lines+1] = '[ch name=\"T' .. i .. '\" text=\"line ' .. i .. '\"]' end\n"
        "lines[#lines+1] = '[p]'\n"
        "lines[#lines+1] = '[end]'\n"
        "local script = table.concat(lines, '\\n')\n"
        "local tokens = tk.parse(script)\n"
        "assert(#tokens >= 30, 'should have 30+ tokens, got ' .. #tokens)\n";
    CHECK(luaL_dostring(L, code) == LUA_OK);
    delete lm;
}
