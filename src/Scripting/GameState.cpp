// ===========================================================================
//  Caesura (AmeKAG) -- GameState.cpp
//  Spec [10.2.31]: Creates the ctx table in the Lua registry with all
//  required fields for KAG script execution.
// ===========================================================================

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "GameState.h"
#include <cstdio>

namespace Caesura {

void GameState::create(lua_State* L) {
    if (!L) return;

    // ctx = {}
    lua_newtable(L);

    // ctx.co = nil           -- 当前协程 (set later by scheduler)
    lua_pushnil(L);
    lua_setfield(L, -2, "co");

    // ctx.tokens = {}        -- Token 序列 (filled by parser)
    lua_newtable(L);
    lua_setfield(L, -2, "tokens");

    // ctx.token_index = 1    -- 当前执行位置
    lua_pushinteger(L, 1);
    lua_setfield(L, -2, "token_index");

    // ctx.call_stack = {}    -- call/return 调用栈
    lua_newtable(L);
    lua_setfield(L, -2, "call_stack");

    // ctx.f = {}             -- 全局变量 (flags)
    lua_newtable(L);
    lua_setfield(L, -2, "f");

    // ctx.sf = {}            -- 系统变量 (system flags)
    lua_newtable(L);
    lua_setfield(L, -2, "sf");

    // ctx.tf = {}            -- 临时变量 (temp flags, scoped per sub-call)
    lua_newtable(L);
    lua_setfield(L, -2, "tf");

    // ctx.layers = {}        -- 图层引用 (bg, fg, message, etc.)
    lua_newtable(L);
    lua_setfield(L, -2, "layers");

    // ctx.backlog = {}       -- 历史消息
    lua_newtable(L);
    lua_setfield(L, -2, "backlog");

    // ctx.active_operations = {}  -- 活跃 CancelToken 列表
    lua_newtable(L);
    lua_setfield(L, -2, "active_operations");

    // ctx.stop_flag = false  -- 脚本终止标志
    lua_pushboolean(L, 0);
    lua_setfield(L, -2, "stop_flag");

    // ctx.input_focus = "kag"  -- 当前输入焦点目标
    lua_pushstring(L, "kag");
    lua_setfield(L, -2, "input_focus");

    // Store ctx in Lua registry: registry["caesura_ctx"] = ctx
    lua_pushstring(L, REGISTRY_KEY);
    lua_pushvalue(L, -2);       // duplicate ctx
    lua_settable(L, LUA_REGISTRYINDEX);

    lua_pop(L, 1);  // pop ctx

    printf("[GameState] ctx table created in Lua registry.\n");
}

bool GameState::push(lua_State* L) {
    if (!L) return false;

    lua_pushstring(L, REGISTRY_KEY);
    if (lua_gettable(L, LUA_REGISTRYINDEX) == LUA_TNIL) {
        lua_pop(L, 1);
        return false;
    }
    return true;
}

} // namespace Caesura
