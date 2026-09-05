#pragma once

#include "doctest.h"
#include "script/vm/LuaManager.h"
#include "script/state/GameState.h"

#include <string>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace u10_test {

inline void runLua(lua_State* state, const char* source) {
    const int stackTop = lua_gettop(state);
    const int result = luaL_dostring(state, source);
    std::string error;
    if (result != LUA_OK) {
        const char* message = lua_tostring(state, -1);
        error = message ? message : "non-string Lua error";
    }
    lua_settop(state, stackTop);
    REQUIRE_MESSAGE(result == LUA_OK, error);
}

// The actual VM, KAG commands, compiler, scheduler, Operation, and layer tree
// are retained. Only scene I/O and external backend effects are substituted.
// Destruction is explicit because LuaManager's destructor does not shut down.
struct SessionFixture {
    Caesura::LuaManager manager;

    ~SessionFixture() { manager.shutdown(); }

    lua_State* state() { return manager.state(); }

    void init() {
        REQUIRE(manager.init());
        runLua(state(), R"lua(
            package.path = 'scripts/?.lua;scripts/?/init.lua;' .. package.path
            u10_load_calls, u10_async_epoch, u10_ai_cancels = 0, 0, 0
            package.loaded.backend = {
                get_resolution = function() return 1280, 720 end,
                cancel_async_loads = function()
                    u10_async_epoch = u10_async_epoch + 1
                end,
                ai_cancel = function() u10_ai_cancels = u10_ai_cancels + 1 end,
                audio_stop = function() end,
            }
            local function load_scene(path)
                u10_load_calls = u10_load_calls + 1
                if path == 'missing.ks' then return nil end
                if path == 'throw.ks' then error('u10 scene preparation failure') end
                local source = '[wait time=60000][end]'
                if path == 'tween.ks' then
                    source = '[tween target=actor attr=x from=0 to=100 dur=1000 wait=true][end]'
                elseif path == 'empty.ks' then
                    source = '[end]'
                elseif path == 'short.ks' then
                    source = '[wait time=1][end]'
                elseif path == 'nonblocking.ks' then
                    source = '[tween target=actor attr=x from=0 to=100 dur=1000 wait=false][wait time=60000][end]'
                end
                return {
                    path = path, tokens = require('tokenizer').parse(source),
                    labels = { source = path },
                }
            end
            package.loaded.flow = {load_scene = load_scene, reload_scene = load_scene}
            require('kag')
            runner = require('kag_runner')
            runner.set_resume_adapter({
                is_paused = function() return false end,
                resume = function(_, co, value)
                    u10_last_co = co
                    return coroutine.resume(co, value)
                end,
            })

            -- Transparent observation of real Lua 5.4 scope finalization.
            -- It calls the original __close with its original arguments;
            -- cancellation and cleanup remain production Operation behavior.
            local operation = require('kag.operation')
            local probe = operation.start({active_operations = {}})
            probe:complete()
            probe:__close()
            local scope = getmetatable(probe)
            local original_close = scope.__close
            u10_close_calls = {}
            scope.__close = function(self, error_value)
                u10_close_calls[self.token] = (u10_close_calls[self.token] or 0) + 1
                return original_close(self, error_value)
            end
        )lua");
    }

    void assertPublished(const char* globalName = "_CAESURA_CTX") {
        lua_State* current = state();
        const int stackTop = lua_gettop(current);
        REQUIRE(Caesura::GameState::push(current));
        REQUIRE(lua_istable(current, -1));
        lua_getglobal(current, globalName);
        CHECK(lua_rawequal(current, -1, -2));
        lua_settop(current, stackTop);
        runLua(current, "assert(rawequal(runner.get_ctx(), _CAESURA_CTX))");
        CHECK(lua_gettop(current) == stackTop);
    }

    void assertIdle() {
        lua_State* current = state();
        const int stackTop = lua_gettop(current);
        CHECK_FALSE(Caesura::GameState::push(current));
        CHECK(lua_gettop(current) == stackTop);
        lua_settop(current, stackTop);
        runLua(current, "assert(runner.get_ctx() == nil and _CAESURA_CTX == nil)");
    }
};

} // namespace u10_test
