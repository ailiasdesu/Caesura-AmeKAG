// ===========================================================================
//  Caesura (AmeKAG) -- GameState.cpp
//  Native reference to the Lua runner's session table.
// ===========================================================================

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "../state/GameState.h"

namespace Caesura {

bool GameState::bind(lua_State* L, int index) {
    if (!L) return false;

    const int stackTop = lua_gettop(L);
    if (index == 0 || index > stackTop || index < -stackTop) return false;

    const int valueType = lua_type(L, index);
    if (valueType != LUA_TTABLE && valueType != LUA_TNIL) return false;

    // Resolve negative indices before pushing the registry key.
    const int valueIndex = lua_absindex(L, index);
    lua_pushstring(L, REGISTRY_KEY);
    lua_pushvalue(L, valueIndex);
    lua_rawset(L, LUA_REGISTRYINDEX);
    return true;
}

bool GameState::push(lua_State* L) {
    if (!L) return false;

    lua_pushstring(L, REGISTRY_KEY);
    lua_rawget(L, LUA_REGISTRYINDEX);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return false;
    }
    return true;
}

bool GameState::stopRunner(lua_State* L) {
    if (!L) return true;
    const int stackTop = lua_gettop(L);
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    if (!lua_istable(L, -1)) { lua_settop(L, stackTop); return true; }
    lua_getfield(L, -1, "kag_runner");
    if (!lua_istable(L, -1)) { lua_settop(L, stackTop); return true; }
    lua_getfield(L, -1, "stop");
    if (!lua_isfunction(L, -1)) { lua_settop(L, stackTop); return false; }
    const bool stopped = lua_pcall(L, 0, 1, 0) == LUA_OK && lua_toboolean(L, -1);
    lua_settop(L, stackTop);
    return stopped;
}

} // namespace Caesura
