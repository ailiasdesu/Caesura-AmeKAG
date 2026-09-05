// test_hotreload_integration.cpp - DebugProtocol features not covered by unit tests
#include "doctest.h"
#include "debug/DebugProtocol.h"
#include "debug/HotReload.h"
#include "U10SessionFixture.h"

#include <chrono>
#include <filesystem>
#include <fstream>

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

TEST_CASE("HotReload selects scene or full reload before invalidating async work") {
    bool luaChanged = false;
    bool luaAdded = false;
    bool sceneAccepted = true;
    SUBCASE("scene-only reload uses its dot-call path") {}
    SUBCASE("failed scene reload keeps pending async work") { sceneAccepted = false; }
    SUBCASE("mixed Lua and scene changes require full reload") { luaChanged = true; }
    SUBCASE("new Lua alongside a scene change requires full reload") { luaAdded = true; }
    const bool fullReload = luaChanged || luaAdded;

    const auto root = std::filesystem::temp_directory_path() /
        ("caesura_reload_async_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);
    const auto scenePath = root / "scene.ks";
    const auto luaPath = root / "helper.lua";
    std::ofstream(scenePath) << "[end]\n";
    if (!luaAdded) std::ofstream(luaPath) << "return {}\n";

    lua_State* L = luaL_newstate();
    REQUIRE(L != nullptr);
    luaL_openlibs(L);
    lua_pushboolean(L, sceneAccepted);
    lua_setglobal(L, "scene_accepted");
    REQUIRE(luaL_dostring(L, R"lua(
        scene_reload_calls, full_reload_calls = 0, 0
        package.preload.kag_runner = function()
            return { reload_scene = function(path)
                assert(type(path) == "string", "scene path must be the first argument")
                assert(not path:find("\\", 1, true), "scene paths use forward slashes")
                scene_reload_calls = scene_reload_calls + 1
                return scene_accepted, "scene-result"
            end }
        end
        package.preload.kag = function()
            full_reload_calls = full_reload_calls + 1
            return {}
        end
    )lua") == LUA_OK);

    HotReload reload;
    reload.init(root.string(), L);
    int cancellationCalls = 0;
    reload.setBeforeReloadCallback([&] { ++cancellationCalls; });
    if (luaAdded) std::ofstream(luaPath) << "return {}\n";
    const auto changedTime = std::filesystem::last_write_time(scenePath) +
        std::chrono::seconds(1);
    std::filesystem::last_write_time(scenePath, changedTime);
    if (luaChanged) std::filesystem::last_write_time(luaPath, changedTime);

    CHECK(reload.checkAndReload());
    CHECK(cancellationCalls == (fullReload ? 1 : 0));
    lua_getglobal(L, "scene_reload_calls");
    CHECK(lua_tointeger(L, -1) == (fullReload ? 0 : 1));
    lua_getglobal(L, "full_reload_calls");
    CHECK(lua_tointeger(L, -1) == (fullReload ? 1 : 0));
    lua_settop(L, 0);
    reload.shutdown();
    lua_close(L);
    std::filesystem::remove_all(root);
}

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

TEST_CASE("HotReload: full reload closes a real runner wait before async invalidation") {
    u10_test::SessionFixture runtime;
    runtime.init();
    auto* L = runtime.state();
    u10_test::runLua(L, R"lua(
        assert(runner.start('A.ks'))
        old_ctx, old_co = runner.get_ctx(), u10_last_co
        old_token = assert(old_ctx.active_operations[1])
        old_cleanup = 0
        old_token:register(function() old_cleanup = old_cleanup + 1 end)
        assert(runner.update(0.025))
        assert(coroutine.status(old_co) == 'suspended')
        assert(not old_token.cancelled and old_cleanup == 0)
        assert(u10_close_calls[old_token] == nil)
    )lua");
    HotReload reload;
    reload.init("__missing_u10_hotreload_directory__", L);
    int invalidations = 0;
    reload.setBeforeReloadCallback([&] {
        ++invalidations;
        u10_test::runLua(L, R"lua(
            assert(coroutine.status(old_co) == 'dead')
            assert(old_token.cancelled and old_cleanup == 1)
            assert(u10_close_calls[old_token] == 1)
            assert(#old_ctx.active_operations == 0)
        )lua");
    });
    reload.requestReload();
    REQUIRE(reload.checkAndReload());
    CHECK(invalidations == 1);
    u10_test::runLua(L, R"lua(
        runner.update(1)
        assert(coroutine.status(old_co) == 'dead')
        assert(old_cleanup == 1 and u10_close_calls[old_token] == 1)
        assert(runner.start('B.ks'))
        assert(runner.get_ctx() ~= old_ctx)
        assert(#runner.get_ctx().active_operations == 1)
        assert(not runner.get_ctx().active_operations[1].cancelled)
    )lua");
    runtime.assertPublished();
    reload.shutdown();
}

TEST_CASE("HotReload: full reload closes a blocking tween and old motion cannot resume") {
    u10_test::SessionFixture runtime;
    runtime.init();
    auto* L = runtime.state();
    u10_test::runLua(L, R"lua(
        local layers = require('layers')
        layers.init()
        actor = layers.add_layer(layers.get_root(), {id='actor', name='actor', x=0, y=0})
        assert(runner.start('tween.ks'))
        old_ctx, old_co = runner.get_ctx(), u10_last_co
        old_token = assert(old_ctx.active_operations[1])
        old_cleanup = 0
        old_token:register(function() old_cleanup = old_cleanup + 1 end)
        assert(runner.update(0.05))
        assert(actor.x > 0 and actor.x < 100, 'real tween must advance before reload')
        frozen_x = actor.x
        assert(coroutine.status(old_co) == 'suspended')
        assert(not old_token.cancelled and old_cleanup == 0)
    )lua");
    HotReload reload;
    reload.init("__missing_u10_hotreload_directory__", L);
    reload.requestReload();
    REQUIRE(reload.checkAndReload());
    u10_test::runLua(L, R"lua(
        assert(coroutine.status(old_co) == 'dead')
        assert(old_token.cancelled and old_cleanup == 1)
        assert(u10_close_calls[old_token] == 1 and #old_ctx.active_operations == 0)
        runner.update(5)
        assert(actor.x == frozen_x, 'closed tween must not reach its old target')
    )lua");
    // A second full reload cannot finalize the old coroutine a second time.
    reload.requestReload();
    REQUIRE(reload.checkAndReload());
    u10_test::runLua(L, R"lua(
        assert(old_cleanup == 1 and u10_close_calls[old_token] == 1)
        assert(runner.start('B.ks'))
        assert(runner.get_ctx() ~= old_ctx)
        runner.update(0.05)
        assert(actor.x == frozen_x)
    )lua");
    runtime.assertPublished();
    reload.shutdown();
}
