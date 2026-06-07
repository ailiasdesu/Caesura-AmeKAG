#include "doctest.h"
#include "Debug/HotReload.h"
#include "Debug/DebugProtocol.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

using namespace Caesura;

TEST_CASE("HotReload::singleton") {
    auto& hr = HotReload::instance();
    CHECK(&hr == &HotReload::instance());
    CHECK_FALSE(hr.initialized());
    CHECK(hr.scriptState() == ScriptState::IDLE);
}

TEST_CASE("HotReload::state transitions") {
    auto& hr = HotReload::instance();
    hr.setScriptState(ScriptState::DEBUG_ACTIVE);
    CHECK(hr.scriptState() == ScriptState::DEBUG_ACTIVE);
    hr.setScriptState(ScriptState::IDLE);
    CHECK(hr.scriptState() == ScriptState::IDLE);
}

TEST_CASE("DebugProtocol::singleton") {
    CHECK(&DebugProtocol::instance() == &DebugProtocol::instance());
}

TEST_CASE("DebugProtocol::breakpoints with init") {
    auto& dp = DebugProtocol::instance();
    lua_State* L = luaL_newstate();
    REQUIRE(L != nullptr);
    dp.init(L);
    
    CHECK_FALSE(dp.isDebugActive());
    dp.setBreakpoint("test.lua", 5);
    CHECK(dp.hasBreakpoint("test.lua", 5));
    CHECK_FALSE(dp.hasBreakpoint("test.lua", 99));
    dp.removeBreakpoint("test.lua", 5);
    CHECK_FALSE(dp.hasBreakpoint("test.lua", 5));
    
    dp.setBreakpoint("a.lua", 1);
    dp.setBreakpoint("b.lua", 2);
    dp.clearAllBreakpoints();
    CHECK_FALSE(dp.hasBreakpoint("a.lua", 1));
    
    lua_close(L);
}