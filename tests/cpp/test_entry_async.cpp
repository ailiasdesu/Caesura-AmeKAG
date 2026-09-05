#include "doctest.h"
#include "entry/Engine.h"
#include "entry/EngineConfig.h"
#include "debug/DebugProtocol.h"
#include "job/api/IJobSystem.h"
#include "script/vm/LuaManager.h"
#include <cstring>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

namespace {

EngineConfig asyncEngineConfig() {
    EngineConfig config;
    config.headless = true;
    config.enableDebugger = true;
    return config;
}

void settleAsyncJobs(Engine& engine) {
    engine.jobSystem().waitIdle();
    engine.jobSystem().pollMainThreadJobs();
}

int luaCount(lua_State* L, const char* name) {
    lua_getglobal(L, name);
    const int count = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    return count;
}

DebugProtocol::PauseId pauseAsyncEngine(Engine& engine) {
    auto* protocol = engine.debugProtocol();
    REQUIRE(protocol != nullptr);
    lua_State* L = engine.lua().state();
    constexpr const char* code = "local value = 1\nvalue = value + 1\nreturn value\n";
    protocol->setBreakpoint("async_debug.lua", 2);
    lua_State* coroutine = lua_newthread(L);
    REQUIRE(luaL_loadbuffer(coroutine, code, std::strlen(code), "async_debug.lua") == LUA_OK);
    int results = 0;
    REQUIRE(lua_resume(coroutine, L, 0, &results) == LUA_YIELD);
    const auto pause = protocol->currentPauseId();
    lua_pop(L, 1); // DebugProtocol anchors the paused coroutine.
    REQUIRE(pause != DebugProtocol::NoPause);
    return pause;
}

} // namespace

TEST_CASE("Entry async: paused completions wait for resume and honour cancellation") {
    bool cancel = false;
    SUBCASE("resume delivers exactly once") {}
    SUBCASE("cancel while paused discards old completion") { cancel = true; }
    Engine engine(asyncEngineConfig());
    REQUIRE(engine.init());
    lua_State* L = engine.lua().state();
    REQUIRE(luaL_dostring(L,
        "async_count = 0; "
        "assert(Render.load_texture_async('__missing_async_pause__.png', function(ok) "
        "assert(not ok); async_count = async_count + 1 end) > 0)") == LUA_OK);
    settleAsyncJobs(engine);
    const auto pause = pauseAsyncEngine(engine);
    const auto commands = engine.debugProtocol()->commandSink();
    int tick = 0;
    engine.run([&] {
        ++tick;
        if (tick == 2) {
            CHECK(luaCount(L, "async_count") == 0);
            if (cancel) REQUIRE(luaL_dostring(L, "Render.cancel_async_loads()") == LUA_OK);
            REQUIRE(commands(pause, DebugProtocol::Command::Continue));
        } else if (tick == 3) {
            CHECK(luaCount(L, "async_count") == 0);
        } else if (tick == 5) {
            CHECK(luaCount(L, "async_count") == (cancel ? 0 : 1));
            engine.quit();
        }
    });
    CHECK(tick == 5);
    CHECK(luaL_dostring(L, "assert(next(_ASYNC_CALLBACKS) == nil)") == LUA_OK);
    engine.shutdown();
}

TEST_CASE("Entry async: full script reload discards old buffered closures") {
    Engine engine(asyncEngineConfig());
    REQUIRE(engine.init());
    lua_State* L = engine.lua().state();
    REQUIRE(luaL_dostring(L,
        "package.path = 'scripts/?.lua;scripts/?/init.lua;' .. package.path; "
        "old_async_count = 0; new_async_count = 0; "
        "assert(Render.load_texture_async('__missing_async_reload__.png', function() "
        "old_async_count = old_async_count + 1 end) > 0)") == LUA_OK);
    settleAsyncJobs(engine);
    REQUIRE(engine.reloadScriptsNow());
    CHECK(luaL_dostring(L, "assert(next(_ASYNC_CALLBACKS) == nil)") == LUA_OK);
    REQUIRE(luaL_dostring(L,
        "assert(Render.load_texture_async('__missing_async_reload__.png', function() "
        "new_async_count = new_async_count + 1 end) > 0)") == LUA_OK);
    settleAsyncJobs(engine);
    int ticks = 0;
    engine.run([&] { if (++ticks == 3) engine.quit(); });
    CHECK(luaCount(L, "old_async_count") == 0);
    CHECK(luaCount(L, "new_async_count") == 1);
    engine.shutdown();
}

TEST_CASE("Entry async: callback cancellation and reenqueue preserve the new closure") {
    Engine engine(asyncEngineConfig());
    REQUIRE(engine.init());
    lua_State* L = engine.lua().state();
    REQUIRE(luaL_dostring(L,
        "first_async_count = 0; stale_async_count = 0; next_async_count = 0; last_async_count = 0; "
        "assert(Render.load_texture_async('__missing_async_reentry__.png', function() "
        " first_async_count = first_async_count + 1; Render.cancel_async_loads(); "
        " assert(Render.load_texture_async('__missing_async_reentry__.png', function() "
        "  next_async_count = next_async_count + 1 end) > 0); "
        " assert(Render.load_texture_async('__missing_async_reentry__.png', function() "
        "  last_async_count = last_async_count + 1 end) > 0) end) > 0); "
        "assert(Render.load_texture_async('__missing_async_reentry__.png', function() "
        " stale_async_count = stale_async_count + 1 end) > 0)") == LUA_OK);
    settleAsyncJobs(engine);
    int ticks = 0;
    engine.run([&] {
        if (++ticks == 2) settleAsyncJobs(engine);
        if (ticks == 4) engine.quit();
    });
    CHECK(luaCount(L, "first_async_count") == 1);
    CHECK(luaCount(L, "stale_async_count") == 0);
    CHECK(luaCount(L, "next_async_count") == 1);
    CHECK(luaCount(L, "last_async_count") == 1);
    CHECK(luaL_dostring(L, "assert(next(_ASYNC_CALLBACKS) == nil)") == LUA_OK);
    engine.shutdown();
}
