// test_hotreload_integration.cpp - hot reload integration tests
#include "doctest.h"
#include "debug/HotReload.h"
#include "debug/DebugProtocol.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

TEST_CASE("HotReload: singleton returns same instance") {
    auto& a = HotReload::instance();
    auto& b = HotReload::instance();
    CHECK(&a == &b);
}

TEST_CASE("HotReload: default state is not initialized") {
    auto& hr = HotReload::instance();
    // State before init
    CHECK(hr.scriptState() == ScriptState::IDLE);
}

TEST_CASE("HotReload: state transitions work") {
    auto& hr = HotReload::instance();

    hr.setScriptState(ScriptState::IDLE);
    CHECK(hr.scriptState() == ScriptState::IDLE);

    hr.setScriptState(ScriptState::DEBUG_ACTIVE);
    CHECK(hr.scriptState() == ScriptState::DEBUG_ACTIVE);

    hr.setScriptState(ScriptState::RELOADING);
    CHECK(hr.scriptState() == ScriptState::RELOADING);

    hr.setScriptState(ScriptState::IDLE);
    CHECK(hr.scriptState() == ScriptState::IDLE);
}

TEST_CASE("DebugProtocol: breakpoints with init") {
    auto& dp = DebugProtocol::instance();
    lua_State* L = luaL_newstate();
    REQUIRE(L != nullptr);

    dp.init(L);
    CHECK_FALSE(dp.isDebugActive());

    dp.setBreakpoint("test.lua", 5);
    CHECK(dp.hasBreakpoint("test.lua", 5));
    CHECK_FALSE(dp.hasBreakpoint("test.lua", 10));

    dp.removeBreakpoint("test.lua", 5);
    CHECK_FALSE(dp.hasBreakpoint("test.lua", 5));

    lua_close(L);
}

TEST_CASE("DebugProtocol: clearAllBreakpoints works") {
    auto& dp = DebugProtocol::instance();
    lua_State* L = luaL_newstate();
    REQUIRE(L != nullptr);
    dp.init(L);

    dp.setBreakpoint("a.lua", 1);
    dp.setBreakpoint("b.lua", 2);
    dp.setBreakpoint("c.lua", 3);
    CHECK(dp.hasBreakpoint("a.lua", 1));
    CHECK(dp.hasBreakpoint("b.lua", 2));

    dp.clearAllBreakpoints();
    CHECK_FALSE(dp.hasBreakpoint("a.lua", 1));
    CHECK_FALSE(dp.hasBreakpoint("b.lua", 2));
    CHECK_FALSE(dp.hasBreakpoint("c.lua", 3));

    lua_close(L);
}
