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

MINIGAME_DISPATCH(spawn_cube, "spawn_cube")
MINIGAME_DISPATCH(spawn_sphere, "spawn_sphere")
MINIGAME_DISPATCH(spawn_plane, "spawn_plane")
MINIGAME_DISPATCH(remove_object, "remove_object")
MINIGAME_DISPATCH(set_camera, "set_camera")
MINIGAME_DISPATCH(create_material, "create_material")
MINIGAME_DISPATCH(set_material, "set_material")
MINIGAME_DISPATCH(set_ambient, "set_ambient")
MINIGAME_DISPATCH(set_directional, "set_directional")
MINIGAME_DISPATCH(add_point_light, "add_point_light")
MINIGAME_DISPATCH(remove_light, "remove_light")
MINIGAME_DISPATCH(check_collision, "check_collision")
MINIGAME_DISPATCH(set_collision, "set_collision")
MINIGAME_DISPATCH(set_velocity, "set_velocity")
MINIGAME_DISPATCH(set_gravity, "set_gravity")

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

static const luaL_Reg mini_game_functions[] = {
    { "spawn_cube",      lua_MiniGame_spawn_cube      },
    { "spawn_sphere",    lua_MiniGame_spawn_sphere    },
    { "spawn_plane",     lua_MiniGame_spawn_plane     },
    { "remove_object",   lua_MiniGame_remove_object   },
    { "set_camera",      lua_MiniGame_set_camera      },
    { "create_material", lua_MiniGame_create_material },
    { "set_material",    lua_MiniGame_set_material    },
    { "set_ambient",     lua_MiniGame_set_ambient     },
    { "set_directional", lua_MiniGame_set_directional },
    { "add_point_light", lua_MiniGame_add_point_light },
    { "remove_light",    lua_MiniGame_remove_light    },
    { "check_collision", lua_MiniGame_check_collision },
    { "set_collision",   lua_MiniGame_set_collision   },
    { "set_velocity",    lua_MiniGame_set_velocity    },
    { "set_gravity",     lua_MiniGame_set_gravity     },
    { "load_scene",      lua_MiniGame_load_scene      },
    { "unload_scene",    lua_MiniGame_unload_scene    },
    { "enter",           lua_MiniGame_enter           },
    { "leave",           lua_MiniGame_leave           },
    { "is_active",       lua_MiniGame_is_active       },
    { nullptr, nullptr }
};

void registerMiniGameBinding(lua_State* L) {
    luaL_newlib(L, mini_game_functions);
    lua_setglobal(L, "mini_game");
    printf("[Lua] MiniGame module registered (20 APIs).\n");
}

} // namespace Caesura
