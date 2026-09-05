// U10: the Lua runner owns the single active session table.
#include "U10SessionFixture.h"

using namespace Caesura;
using u10_test::runLua;

TEST_CASE("GameState: idle and malformed references are absent without stack growth") {
    u10_test::SessionFixture runtime;
    runtime.init();
    runtime.assertIdle();
    auto* L = runtime.state();
    CHECK_FALSE(GameState::push(nullptr));
    for (const char* invalid : {"nil", "false", "17", "'not a context'"}) {
        // Corruption probe only: production never uses debug.getregistry().
        const std::string assignment =
            std::string("debug.getregistry().caesura_ctx = ") + invalid;
        runLua(L, assignment.c_str());
        const int stackTop = lua_gettop(L);
        CHECK_FALSE(GameState::push(L));
        CHECK(lua_gettop(L) == stackTop);
        lua_settop(L, stackTop);
    }
    runLua(L, "debug.getregistry().caesura_ctx = nil");
}

TEST_CASE("GameState: start publishes one table and rejected start preserves it") {
    u10_test::SessionFixture runtime;
    runtime.init();
    auto* L = runtime.state();
    runLua(L, R"lua(
        assert(runner.start('A.ks'))
        a = runner.get_ctx()
        local loads, epoch = u10_load_calls, u10_async_epoch
        local started = runner.start('B.ks')
        assert(started == false)
        assert(rawequal(a, runner.get_ctx()) and rawequal(a, _CAESURA_CTX))
        assert(u10_load_calls == loads and u10_async_epoch == epoch)
        assert(coroutine.status(u10_last_co) == 'suspended')
    )lua");
    runtime.assertPublished("a");
    // VM initialization is idempotent and must not replace the live context.
    CHECK(runtime.manager.init());
    runtime.assertPublished("a");
}

TEST_CASE("GameState: backlog and custom fields are shared in both directions") {
    u10_test::SessionFixture runtime;
    runtime.init();
    auto* L = runtime.state();
    runLua(L, R"lua(
        assert(runner.start('A.ks'))
        a = runner.get_ctx()
        assert(type(a.backlog) == 'table')
        a.backlog[1] = {text = 'observed dialogue'}
        a.lua_value = 'written by runner'
    )lua");
    REQUIRE(GameState::push(L));
    lua_getfield(L, -1, "lua_value");
    REQUIRE(lua_isstring(L, -1));
    CHECK(std::string(lua_tostring(L, -1)) == "written by runner");
    lua_pop(L, 1);
    lua_getfield(L, -1, "backlog");
    REQUIRE(lua_istable(L, -1));
    CHECK(lua_rawlen(L, -1) == 1);
    lua_setglobal(L, "cpp_backlog");
    lua_pushstring(L, "written by C++");
    lua_setfield(L, -2, "cpp_value");
    lua_pop(L, 1);
    runLua(L, R"lua(
        assert(a.cpp_value == 'written by C++')
        assert(rawequal(cpp_backlog, a.backlog))
        assert(cpp_backlog[1].text == 'observed dialogue')
    )lua");
    runtime.assertPublished("a");
}

TEST_CASE("GameState: failed start leaves no phantom session and a retry can run") {
    u10_test::SessionFixture runtime;
    runtime.init();
    auto* L = runtime.state();
    SUBCASE("missing initial scene") {
        runLua(L, R"lua(
            local epoch = u10_async_epoch
            assert(runner.start('missing.ks') == false)
            assert(u10_async_epoch == epoch)
        )lua");
    }
    SUBCASE("throwing initial scene loader") {
        runLua(L, R"lua(
            local epoch = u10_async_epoch
            local called, started = pcall(runner.start, 'throw.ks')
            assert(called and started == false)
            assert(u10_async_epoch == epoch)
        )lua");
    }
    SUBCASE("adapter fails after actually suspending the scheduler") {
        runLua(L, R"lua(
            runner.set_resume_adapter({
                is_paused = function() return false end,
                resume = function(_, co, value)
                    u10_failed_co = co
                    assert(coroutine.resume(co, value))
                    u10_failed_ctx = runner.get_ctx()
                    u10_failed_token = assert(u10_failed_ctx.active_operations[1])
                    return false, 'u10 adapter failure after resume'
                end,
            })
            assert(runner.start('A.ks') == false)
            assert(coroutine.status(u10_failed_co) == 'dead')
            assert(u10_failed_token.cancelled)
            assert(u10_close_calls[u10_failed_token] == 1)
            assert(#u10_failed_ctx.active_operations == 0)
            runner.set_resume_adapter(nil)
        )lua");
    }
    runtime.assertIdle();
    runLua(L, "assert(runner.start('B.ks'))");
    runtime.assertPublished();
}

TEST_CASE("GameState: separate VMs have independent active contexts and fields") {
    u10_test::SessionFixture first;
    u10_test::SessionFixture second;
    first.init();
    second.init();
    runLua(first.state(), "assert(runner.start('A.ks')); runner.get_ctx().custom_key = 'first VM'");
    runLua(second.state(), "assert(runner.start('B.ks')); assert(runner.get_ctx().custom_key == nil)");
    first.assertPublished();
    second.assertPublished();
    runLua(first.state(), "assert(runner.get_ctx().custom_key == 'first VM'); assert(runner.stop())");
    first.assertIdle();
    second.assertPublished();
    runLua(second.state(), "assert(runner.get_ctx().current_scene == 'B.ks')");
}

TEST_CASE("GameState: stop closes once and an old scene callback cannot affect its successor") {
    u10_test::SessionFixture runtime;
    runtime.init();
    auto* L = runtime.state();
    runLua(L, R"lua(
        assert(runner.start('A.ks'))
        a, a_co = runner.get_ctx(), u10_last_co
        a_token = assert(a.active_operations[1])
        local late_a_callback = function() return a.load_tokens('late-A.ks') end
        local cleanup_count, reentrant_started = 0, nil
        a_token:register(function()
            cleanup_count = cleanup_count + 1
            reentrant_started = runner.start('reentrant.ks')
        end)
        local epoch = u10_async_epoch
        assert(runner.stop())
        assert(coroutine.status(a_co) == 'dead' and a_token.cancelled)
        assert(#a.active_operations == 0 and u10_close_calls[a_token] == 1)
        assert(cleanup_count == 1 and reentrant_started == false)
        assert(u10_async_epoch > epoch)
        assert(runner.get_ctx() == nil and _CAESURA_CTX == nil)
        runner.stop() -- Repeated stop is safe, without duplicate cleanup.
        assert(cleanup_count == 1 and u10_close_calls[a_token] == 1)

        assert(runner.start('B.ks'))
        b, b_co = runner.get_ctx(), u10_last_co
        assert(a ~= b and b.labelMap.source == 'B.ks')
        local loads, b_epoch = u10_load_calls, u10_async_epoch
        assert(late_a_callback() == nil)
        assert(u10_load_calls == loads, 'stale scene callback must not perform scene I/O')
        assert(u10_async_epoch == b_epoch)
        assert(b.labelMap.source == 'B.ks' and not b._scene_changed)
        assert(rawequal(b, runner.get_ctx()) and coroutine.status(b_co) == 'suspended')
        assert(#b.active_operations == 1 and not b.active_operations[1].cancelled)
    )lua");
    runtime.assertPublished("b");
}

TEST_CASE("GameState: bind preserves stack and reference across invalid values and indices") {
    u10_test::SessionFixture runtime;
    REQUIRE(runtime.manager.init());
    auto* L = runtime.state();
    CHECK_FALSE(GameState::bind(nullptr, 1));
    CHECK_FALSE(GameState::bind(L, -1)); // Empty stack must be rejected before lua_type.
    CHECK(lua_gettop(L) == 0);

    lua_newtable(L);
    lua_pushliteral(L, "stack sentinel");
    const int initialTop = lua_gettop(L);
    REQUIRE(GameState::bind(L, -2));
    CHECK(lua_gettop(L) == initialTop);

    const auto checkReference = [&] {
        const int stackTop = lua_gettop(L);
        REQUIRE(GameState::push(L));
        CHECK(lua_rawequal(L, 1, -1));
        lua_pop(L, 1);
        CHECK(lua_gettop(L) == stackTop);
        CHECK(lua_istable(L, 1));
        REQUIRE(lua_isstring(L, 2));
        CHECK(std::string(lua_tostring(L, 2)) == "stack sentinel");
    };
    checkReference();

    // These are rejected before calling Lua with an invalid stack index.
    // The bridge deliberately accepts stack values, not Lua pseudo-indices.
    const int invalidIndices[] = {0, initialTop + 1, -initialTop - 1, LUA_REGISTRYINDEX};
    for (const int index : invalidIndices) {
        CAPTURE(index);
        CHECK_FALSE(GameState::bind(L, index));
        CHECK(lua_gettop(L) == initialTop);
        checkReference();
    }

    for (const int type : {LUA_TBOOLEAN, LUA_TNUMBER, LUA_TSTRING}) {
        CAPTURE(type);
        if (type == LUA_TBOOLEAN) lua_pushboolean(L, 0);
        else if (type == LUA_TNUMBER) lua_pushinteger(L, 17);
        else lua_pushliteral(L, "not a context");
        CHECK_FALSE(GameState::bind(L, -1));
        CHECK(lua_gettop(L) == initialTop + 1);
        CHECK(lua_type(L, -1) == type);
        checkReference();
        lua_pop(L, 1);
    }

    lua_pushnil(L);
    REQUIRE(GameState::bind(L, -1));
    CHECK(lua_gettop(L) == initialTop + 1);
    CHECK(lua_isnil(L, -1));
    CHECK_FALSE(GameState::push(L));
    CHECK(lua_gettop(L) == initialTop + 1);
    lua_pop(L, 1);

    REQUIRE(GameState::bind(L, 1));
    CHECK(lua_gettop(L) == initialTop);
    checkReference();
    lua_settop(L, 0);
}

TEST_CASE("GameState: natural end retains final state but retires session work") {
    u10_test::SessionFixture runtime;
    runtime.init();
    runLua(runtime.state(), R"lua(
        assert(runner.start('short.ks'))
        a, a_co = runner.get_ctx(), u10_last_co
        a.f.answer = 42
        local loads, epoch, ai = u10_load_calls, u10_async_epoch, u10_ai_cancels
        for i = 1, 4 do runner.update(0.01) end
        assert(coroutine.status(a_co) == 'dead' and a.co == nil)
        assert(rawequal(a, runner.get_ctx()) and a.f.answer == 42)
        assert(#a.active_operations == 0)
        assert(u10_async_epoch == epoch + 1 and u10_ai_cancels == ai + 1)
        assert(a.load_tokens('late.ks') == nil and u10_load_calls == loads)
        assert(not pcall(require('kag.operation').start, a))
        for i = 1, 4 do runner.update(0.01) end
        assert(u10_async_epoch == epoch + 1 and u10_ai_cancels == ai + 1)
        local running, reason = runner.update(0.01)
        assert(running == false and reason == 'ended')
    )lua");
    runtime.assertPublished("a");
}

TEST_CASE("GameState: same scene reload cancels old motion and preserves table identity") {
    u10_test::SessionFixture runtime;
    runtime.init();
    runLua(runtime.state(), R"lua(
        local layers = require('layers')
        layers.init()
        actor = layers.add_layer(layers.get_root(), {id='actor', name='actor', x=0})
        assert(runner.start('nonblocking.ks'))
        a, a_co = runner.get_ctx(), u10_last_co
        assert(runner.update(0.05))
        assert(actor.x > 0 and actor.x < 100 and #a.tweens == 1)
        local old_tween, frozen_x = a.tweens[1], actor.x
        local frozen_time = old_tween.t
        local token = assert(a.active_operations[1])
        local cleanup, new_operation = 0, nil
        token:register(function()
            cleanup = cleanup + 1
            new_operation = pcall(require('kag.operation').start, a)
            assert(runner.start('forbidden.ks') == false)
            assert(runner.update(1) == false)
        end)
        assert(runner.reload_scene('nonblocking.ks'))
        assert(rawequal(a, runner.get_ctx()))
        assert(old_tween.cancelled and #a.tweens == 0)
        assert(actor.x == frozen_x and old_tween.t == frozen_time)
        assert(token.cancelled and cleanup == 1 and new_operation == false)
        assert(coroutine.status(a_co) == 'dead')
        assert(runner.update(0.05))
        assert(runner.update(0.05))
        assert(old_tween.t == frozen_time and old_tween.cancelled)
        assert(not a.tweens[1] or a.tweens[1] ~= old_tween)
        assert(#a.active_operations == 1)
        assert(a.co == u10_last_co and a.co ~= a_co)
    )lua");
    runtime.assertPublished("a");
}

TEST_CASE("GameState: failed replacement preserves a pending scene reload") {
    u10_test::SessionFixture runtime;
    runtime.init();
    runLua(runtime.state(), R"lua(
        assert(runner.start('A.ks'))
        a, a_co = runner.get_ctx(), u10_last_co
        assert(runner.reload_scene('A.ks'))
        assert(a._pendingSceneReload and a.co == nil)
        assert(runner.start('missing.ks') == false)
        assert(a._pendingSceneReload and a.co == nil and rawequal(a, runner.get_ctx()))
        assert(runner.update(0.01))
        assert(a.co ~= a_co and coroutine.status(a.co) == 'suspended')
        assert(#a.active_operations == 1 and not a.active_operations[1].cancelled)
    )lua");
    runtime.assertPublished("a");
}

TEST_CASE("GameState: invalid deferred jumps retire session work exactly once") {
    u10_test::SessionFixture runtime;
    runtime.init();
    const char* target = "*missing_label";
    SUBCASE("missing label") {}
    SUBCASE("unsafe scene") { target = "../../outside.ks"; }
    lua_pushstring(runtime.state(), target);
    lua_setglobal(runtime.state(), "bad_jump");
    runLua(runtime.state(), R"lua(
        local layers = require('layers')
        layers.init()
        layers.add_layer(layers.get_root(), {id='actor', name='actor', x=0})
        assert(runner.start('nonblocking.ks'))
        a = runner.get_ctx()
        assert(runner.update(0.01))
        local tween, epoch, ai = a.tweens[1], u10_async_epoch, u10_ai_cancels
        a._pendingJump, a.stop_flag = bad_jump, true
        local running, reason
        for i = 1, 8 do
            running, reason = runner.update(0.01)
            if not running then break end
        end
        assert(running == false and (reason == 'label-not-found' or reason == 'unsafe-jump-target'))
        assert(a.co == nil and a.stop_flag and a._session_active == false)
        assert(tween.cancelled and #a.tweens == 0 and #a.active_operations == 0)
        assert(u10_async_epoch == epoch + 1 and u10_ai_cancels == ai + 1)
        local loads = u10_load_calls
        assert(a.load_tokens('late.ks') == nil and u10_load_calls == loads)
        runner.update(1)
        assert(u10_async_epoch == epoch + 1 and u10_ai_cancels == ai + 1)
    )lua");
    runtime.assertPublished("a");
}
