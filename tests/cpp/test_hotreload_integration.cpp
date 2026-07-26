// test_hotreload_integration.cpp - DebugProtocol features not covered by unit tests
#include "doctest.h"
#include "debug/DebugProtocol.h"
#include "debug/HotReload.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

using namespace Caesura;

namespace {

int g_countHookCalls = 0;
int g_tailCallHookCalls = 0;

void countingHook(lua_State*, lua_Debug* ar) {
    if (ar && ar->event == LUA_HOOKCOUNT) {
        ++g_countHookCalls;
    } else if (ar && ar->event == LUA_HOOKTAILCALL) {
        ++g_tailCallHookCalls;
    }
}

void replacementHook(lua_State*, lua_Debug*) {}

} // namespace

TEST_CASE("DebugProtocol preserves Lua hook ownership") {
    HotReload hr;
    lua_State* L = luaL_newstate();
    REQUIRE(L != nullptr);
    luaL_openlibs(L);
    hr.init("__missing_hotreload_dir__", L);
    lua_sethook(L, countingHook, LUA_MASKCALL | LUA_MASKCOUNT, 1000);

    const lua_Hook originalHook = lua_gethook(L);
    const int originalMask = lua_gethookmask(L);
    const int originalCount = lua_gethookcount(L);

    DebugProtocol dp(hr);
    REQUIRE(dp.init(L));
    CHECK(dp.init(L));
    CHECK(lua_gethook(L) != originalHook);
    CHECK(lua_gethookmask(L) == (originalMask | LUA_MASKLINE));
    CHECK(lua_gethookcount(L) == originalCount);

    DebugProtocol competing(hr);
    CHECK_FALSE(competing.init(L));

    dp.setBreakpoint("a.lua", 1);
    dp.setBreakpoint("b.lua", 2);
    dp.setBreakpoint("c.lua", 3);
    CHECK(dp.hasBreakpoint("a.lua", 1));
    CHECK(dp.hasBreakpoint("b.lua", 2));

    dp.clearAllBreakpoints();
    CHECK_FALSE(dp.hasBreakpoint("a.lua", 1));
    CHECK_FALSE(dp.hasBreakpoint("b.lua", 2));
    CHECK_FALSE(dp.hasBreakpoint("c.lua", 3));

    g_countHookCalls = 0;
    const int shortScriptStatus =
        luaL_dostring(L, "local a = 1\nlocal b = 2\nlocal c = a + b");
    REQUIRE(shortScriptStatus == LUA_OK);
    CHECK(g_countHookCalls == 0);

    g_tailCallHookCalls = 0;
    const int tailCallStatus = luaL_dostring(L,
        "local function tail(n) if n == 0 then return 0 end "
        "return tail(n - 1) end return tail(4)");
    REQUIRE(tailCallStatus == LUA_OK);
    CHECK(g_tailCallHookCalls > 0);
    lua_settop(L, 0);

    const int longScriptStatus = luaL_dostring(
        L, "local total = 0; for i = 1, 10000 do total = total + i end");
    REQUIRE(longScriptStatus == LUA_OK);
    CHECK(g_countHookCalls > 0);

    dp.shutdown();
    CHECK(lua_gethook(L) == originalHook);
    CHECK(lua_gethookmask(L) == originalMask);
    CHECK(lua_gethookcount(L) == originalCount);

    REQUIRE(dp.init(L));
    lua_sethook(L, replacementHook, LUA_MASKCALL, 0);
    dp.shutdown();
    CHECK(lua_gethook(L) == replacementHook);
    CHECK(lua_gethookmask(L) == LUA_MASKCALL);
    CHECK(lua_gethookcount(L) == 0);

    hr.shutdown();
    lua_close(L);
}
