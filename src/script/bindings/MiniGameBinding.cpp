// MiniGameBinding.cpp — Lua `mini_game` global for the 3D mini-game backend.
// Lifecycle methods (load_scene/enter/leave/is_active/unload_scene) dispatch
// directly on IMiniGameBackend; object/light/material/camera methods dispatch
// through IMiniGameBackend::luaCall, which expects the method name at stack
// position 1 and the arguments from position 2 (see BgfxMiniGameBackend).
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "MiniGameBinding.h"
#include "../../minigame/api/IMiniGameBackend.h"
#include "../../minigame/api/MiniGameCommands.h"
#include <cstdio>

namespace Caesura {

// ===========================================================================
// Helper: IMiniGameBackend from Lua registry (set by Engine_LuaRegistry)
// ===========================================================================

static IMiniGameBackend* getMiniGame(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "Caesura.MiniGameBackend");
    auto* mg = static_cast<IMiniGameBackend*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return mg;
}

// ===========================================================================
// luaCall dispatcher: pushes the method name to stack position 1, then lets
// the backend parse args from position 2. Returns the backend's result count.
// ===========================================================================

static int lua_MiniGame_dispatch(lua_State* L, const char* method) {
    IMiniGameBackend* mg = getMiniGame(L);
    if (!mg) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "No mini-game backend registered");
        return 2;
    }
    lua_pushstring(L, method);
    lua_insert(L, 1);  // method name -> stack position 1, args shift up
    return mg->luaCall(L, method);
}

#define MINIGAME_DISPATCH(name, method)                                      \
    static int lua_MiniGame_##name(lua_State* L) {                           \
        return lua_MiniGame_dispatch(L, method);                             \
    }

// Command dispatch stubs are generated from the single source of truth
// (CAESURA_MINIGAME_COMMANDS in MiniGameCommands.h), so the Lua-visible names
// cannot drift from the backend table. Each expansion makes:
//     static int lua_MiniGame_spawn_cube(lua_State* L) { ... }
#define CAESURA_MG_STUB(snake, camel) MINIGAME_DISPATCH(snake, #snake)
CAESURA_MINIGAME_COMMANDS(CAESURA_MG_STUB)
#undef CAESURA_MG_STUB

// ===========================================================================
// Lifecycle methods (not part of luaCall)
// ===========================================================================

static int lua_MiniGame_load_scene(lua_State* L) {
    IMiniGameBackend* mg = getMiniGame(L);
    if (!mg) { lua_pushinteger(L, 0); return 1; }
    const char* path = luaL_checkstring(L, 1);
    lua_pushinteger(L, static_cast<lua_Integer>(mg->loadScene(path)));
    return 1;
}

static int lua_MiniGame_unload_scene(lua_State* L) {
    IMiniGameBackend* mg = getMiniGame(L);
    if (!mg) { lua_pushboolean(L, 0); return 1; }
    mg->unloadScene(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_MiniGame_enter(lua_State* L) {
    IMiniGameBackend* mg = getMiniGame(L);
    if (!mg) { lua_pushboolean(L, 0); return 1; }
    mg->enter(static_cast<uint32_t>(luaL_optinteger(L, 1, 0)));
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_MiniGame_leave(lua_State* L) {
    IMiniGameBackend* mg = getMiniGame(L);
    if (!mg) { lua_pushboolean(L, 0); return 1; }
    mg->leave();
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_MiniGame_is_active(lua_State* L) {
    IMiniGameBackend* mg = getMiniGame(L);
    lua_pushboolean(L, mg && mg->isActive() ? 1 : 0);
    return 1;
}

// ===========================================================================

// Command rows are generated from the single source of truth
// (CAESURA_MINIGAME_COMMANDS in MiniGameCommands.h); the dispatch-stub names
// are exactly lua_MiniGame_##snake (snake_case), so registration stays aligned.
#define CAESURA_MG_REG(snake, camel) { #snake, lua_MiniGame_##snake },
static const luaL_Reg mini_game_functions[] = {
    CAESURA_MINIGAME_COMMANDS(CAESURA_MG_REG)
    { "load_scene",      lua_MiniGame_load_scene   },
    { "unload_scene",    lua_MiniGame_unload_scene },
    { "enter",           lua_MiniGame_enter        },
    { "leave",           lua_MiniGame_leave        },
    { "is_active",       lua_MiniGame_is_active    },
    { nullptr, nullptr }
};
#undef CAESURA_MG_REG

void registerMiniGameBinding(lua_State* L) {
    luaL_newlib(L, mini_game_functions);
    lua_setglobal(L, "mini_game");
    printf("[Lua] MiniGame module registered (20 APIs).\n");
}

} // namespace Caesura
