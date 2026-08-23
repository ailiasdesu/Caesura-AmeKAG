 extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "DevCoreBinding.h"
#include "../../input/api/IInputRouter.h"
#include "../../platform/api/IPlatformBackend.h"
#include "../../platform/api/IDisplayService.h"
#include "../../di/BackendRegistry.h"
#include "../../render/api/IRenderDevice.h"
#include <cstdio>
#include <cstring>

namespace Caesura {

// -- Helpers ----------------------------------------------------------------

static IInputRouter* getInput(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "Caesura.InputRouter");
    auto* router = (IInputRouter*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return router;
}

static IRenderDevice* getRender(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "Caesura.RenderDevice");
    auto* dev = (IRenderDevice*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return dev;
}

static IPlatformBackend* getPlatform(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "Caesura.PlatformBackend");
    auto* pb = (IPlatformBackend*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return pb;
}

// -- DevCore.set_input_focus(mode) ----------------------------------------

static int lua_DevCore_set_input_focus(lua_State* L) {
    if (lua_type(L, 1) != LUA_TSTRING) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Focus must be 'KAG' or 'GAME'.");
        return 2;
    }
    const char* mode = lua_tostring(L, 1);

    IInputRouter* router = getInput(L);
    if (!router) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Input router is unavailable.");
        return 2;
    }

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
    IInputRouter* router = getInput(L);
    if (!router) { lua_pushstring(L, "KAG"); return 1; }

    lua_pushstring(L, router->getFocus() == InputFocus::GAME ? "GAME" : "KAG");
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
    auto* renderer = getRender(L);
    auto* platform = getPlatform(L);
    if (!renderer) { lua_pushboolean(L, 0); return 1; }
    // Resize renderer (bgfx::reset + view re-setup)
    renderer->resize(w, h);
    // Resize SDL window if platform backend available
    if (platform) {
        platform->resizeWindow(w, h);
    }
    printf("[DevCore] Resolution set: %dx%d\n", w, h);
    lua_pushboolean(L, 1);
    return 1;
}

// -- DevCore.get_resolution() ------------------------------------------------------

static int lua_DevCore_get_resolution(lua_State* L) {
    auto* renderer = getRender(L);
    if (!renderer) { lua_pushinteger(L, 0); lua_pushinteger(L, 0); return 2; }
    lua_pushinteger(L, renderer->getBackbufferWidth());
    lua_pushinteger(L, renderer->getBackbufferHeight());
    return 2;
}

// -- DevCore.set_fullscreen(enabled) ------------------------------------

static int lua_DevCore_set_fullscreen(lua_State* L) {
    int enabled = lua_toboolean(L, 1);
    auto* platform = getPlatform(L);
    if (!platform) { lua_pushboolean(L, 0); return 1; }
    platform->setFullscreen(enabled);
    printf("[DevCore] Fullscreen: %s\n", enabled ? "ON" : "OFF");
    lua_pushboolean(L, 1);
    return 1;
}


// -- DevCore.get_window_size() -- alias for get_resolution -----------------

static int lua_DevCore_get_window_size(lua_State* L) {
    return lua_DevCore_get_resolution(L);
}
// -- DevCore.get_display_metrics() ----------------------------------------
// Track P1: live display metrics from IDisplayService (registered by the
// composition root). Returns a table or nil when no service is installed.
static const char* orientationName(Caesura::Orientation o) {
    using namespace Caesura;
    switch (o) {
        case Orientation::Portrait: return "portrait";
        case Orientation::PortraitUpsideDown: return "portrait_upside_down";
        case Orientation::LandscapeLeft: return "landscape_left";
        case Orientation::LandscapeRight: return "landscape_right";
        default: return "unknown";
    }
}

static int lua_DevCore_get_display_metrics(lua_State* L) {
    auto* svc = Caesura::BackendRegistry::instance().getDisplayService();
    if (!svc) { lua_pushnil(L); return 1; }
    const auto m = svc->currentMetrics();
    lua_newtable(L);
    lua_pushinteger(L, (lua_Integer)m.pixelWidth);
    lua_setfield(L, -2, "pixelWidth");
    lua_pushinteger(L, (lua_Integer)m.pixelHeight);
    lua_setfield(L, -2, "pixelHeight");
    lua_pushinteger(L, (lua_Integer)m.logicalWidth);
    lua_setfield(L, -2, "logicalWidth");
    lua_pushinteger(L, (lua_Integer)m.logicalHeight);
    lua_setfield(L, -2, "logicalHeight");
    lua_pushnumber(L, (lua_Number)m.scaleFactor);
    lua_setfield(L, -2, "scaleFactor");
    lua_pushnumber(L, (lua_Number)m.dpi);
    lua_setfield(L, -2, "dpi");
    lua_pushstring(L, orientationName(m.orientation));
    lua_setfield(L, -2, "orientation");
    lua_newtable(L);
    lua_pushnumber(L, (lua_Number)m.safeArea.left);
    lua_setfield(L, -2, "left");
    lua_pushnumber(L, (lua_Number)m.safeArea.top);
    lua_setfield(L, -2, "top");
    lua_pushnumber(L, (lua_Number)m.safeArea.right);
    lua_setfield(L, -2, "right");
    lua_pushnumber(L, (lua_Number)m.safeArea.bottom);
    lua_setfield(L, -2, "bottom");
    lua_setfield(L, -2, "safeArea");
    return 1;
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
    { "get_display_metrics", lua_DevCore_get_display_metrics },
    { nullptr, nullptr }
};

void registerDevCoreBinding(lua_State* L) {
    luaL_newlib(L, devcore_functions);
    lua_setglobal(L, "DevCore");
    printf("[Lua] DevCore module registered (via BackendRegistry).\n");
}

} // namespace Caesura