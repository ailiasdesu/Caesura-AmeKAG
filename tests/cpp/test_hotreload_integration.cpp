// test_hotreload_integration.cpp - DebugProtocol features not covered by unit tests
#include "doctest.h"
#include "debug/DebugProtocol.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

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
