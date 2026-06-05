 extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "DevCoreBinding.h"
#include "../Core/BackendRegistry.h"
#include "../Core/InputRouter.h"
#include "../Core/SDL3PlatformBackend.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>

namespace Caesura {

// -- Helper ----------------------------------------------------------------

static InputRouter* getInput(lua_State* L) {
    return BackendRegistry::getInputRouterFromLua(L);
}

// -- DevCore.set_input_focus(mode) ----------------------------------------

static int lua_DevCore_set_input_focus(lua_State* L) {
    const char* mode = luaL_checkstring(L, 1);

    InputRouter* router = getInput(L);
    if (!router) { lua_pushboolean(L, 0); return 1; }

    if (strcmp(mode, "KAG") == 0 || strcmp(mode, "kag") == 0) {
        router->setFocus(InputFocus::KAG);
        printf("[DevCore] Input focus -> KAG\n");
    } else if (strcmp(mode, "GAME") == 0 || strcmp(mode, "game") == 0) {
        router->setFocus(InputFocus::GAME);
        printf("[DevCore] Input focus -> GAME\n");
    } else {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Invalid focus. Use 'KAG' or 'GAME'.");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

// -- DevCore.get_input_focus() --------------------------------------------

static int lua_DevCore_get_input_focus(lua_State* L) {
    InputRouter* router = getInput(L);
    if (!router) { lua_pushstring(L, "KAG"); return 1; }

    lua_pushstring(L, inputFocusToString(router->getFocus()));
    return 1;
}

// -- DevCore.log(msg) -----------------------------------------------------

static int lua_DevCore_log(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    printf("[DevCore] %s\n", msg);
    return 0;
}


// -- DevCore.quit() -------------------------------------------------------

static int lua_DevCore_quit(lua_State* L) {
    (void)L;
    printf("[DevCore] Quit requested from Lua.\n");
    lua_pushboolean(L, 1);
    lua_setglobal(L, "_CAESURA_QUIT");
    return 0;
}

// -- DevCore.set_resolution(width, height) ----------------------------------------

static int lua_DevCore_set_resolution(lua_State* L) {
    int w = (int)luaL_checkinteger(L, 1);
    int h = (int)luaL_checkinteger(L, 2);
    if (w <= 0 || h <= 0) { lua_pushboolean(L, 0); return 1; }
    auto* renderer = BackendRegistry::getRenderDeviceFromLua(L);
    auto* platform = BackendRegistry::getPlatformBackendFromLua(L);
    if (!renderer) { lua_pushboolean(L, 0); return 1; }
    // Resize renderer (bgfx::reset + view re-setup)
    renderer->resize(w, h);
    // Resize SDL window if platform backend available
    if (platform) {
        SDL_Window* win = static_cast<SDL3PlatformBackend*>(platform)->window();
        if (win) SDL_SetWindowSize(win, w, h);
    }
    printf("[DevCore] Resolution set: %dx%d\n", w, h);
    lua_pushboolean(L, 1);
    return 1;
}

// -- DevCore.get_resolution() ------------------------------------------------------

static int lua_DevCore_get_resolution(lua_State* L) {
    auto* renderer = BackendRegistry::getRenderDeviceFromLua(L);
    if (!renderer) { lua_pushinteger(L, 0); lua_pushinteger(L, 0); return 2; }
    lua_pushinteger(L, renderer->getBackbufferWidth());
    lua_pushinteger(L, renderer->getBackbufferHeight());
    return 2;
}

// -- DevCore.set_fullscreen(enabled) ------------------------------------

static int lua_DevCore_set_fullscreen(lua_State* L) {
    int enabled = lua_toboolean(L, 1);
    auto* platform = BackendRegistry::getPlatformBackendFromLua(L);
    if (!platform) { lua_pushboolean(L, 0); return 1; }
    SDL_Window* win = static_cast<SDL3PlatformBackend*>(platform)->window();
    if (win) {
        SDL_SetWindowFullscreen(win, enabled ? SDL_WINDOW_FULLSCREEN : 0);
        printf("[DevCore] Fullscreen: %s\n", enabled ? "ON" : "OFF");
    }
    lua_pushboolean(L, 1);
    return 1;
}


// -- DevCore.get_window_size() -- alias for get_resolution -----------------

static int lua_DevCore_get_window_size(lua_State* L) {
    return lua_DevCore_get_resolution(L);
}
// -- Module registration --------------------------------------------------

static const luaL_Reg devcore_functions[] = {
    { "set_input_focus", lua_DevCore_set_input_focus },
    { "get_input_focus", lua_DevCore_get_input_focus },
    { "log",             lua_DevCore_log             },
    { "quit",            lua_DevCore_quit            },
    { "set_resolution",  lua_DevCore_set_resolution  },
    { "get_resolution",  lua_DevCore_get_resolution  },
    { "set_fullscreen",   lua_DevCore_set_fullscreen },
    { "get_window_size",  lua_DevCore_get_window_size },
    { nullptr, nullptr }
};

void registerDevCoreBinding(lua_State* L) {
    luaL_newlib(L, devcore_functions);
    lua_setglobal(L, "DevCore");
    printf("[Lua] DevCore module registered (via BackendRegistry).\n");
}

} // namespace Caesura
