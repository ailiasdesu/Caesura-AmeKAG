// EngineBinding.cpp — Lua "Engine" module implementation (P1-5 migration
// from di/BackendRegistry). Backend creation still flows through the
// registry (createRenderDevice/...); only the Lua surface lives here.
#include "EngineBinding.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "../../di/BackendRegistry.h"
#include "../../render/api/IRenderDevice.h"
#include "../../audio/api/IAudioBackend.h"
#include "../../platform/api/IPlatformBackend.h"
#include "../state/GameState.h"
#include <cstdio>

namespace Caesura {
namespace engine_binding {

// Registry keys for lightuserdata (same names the old registry code used).
static const char* kRegistryKey_RenderDevice    = "Caesura.RenderDevice";
static const char* kRegistryKey_AudioBackend    = "Caesura.AudioBackend";
static const char* kRegistryKey_PlatformBackend = "Caesura.PlatformBackend";

static int lua_Engine_select_render_backend(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    const char* subBackend = luaL_optstring(L, 2, nullptr);

    auto& registry = BackendRegistry::instance();
    IRenderDevice* device = registry.createRenderDevice(name);
    if (!device) {
        lua_pushnil(L);
        lua_pushfstring(L, "Unknown render backend: %s", name);
        return 2;
    }

    lua_pushlightuserdata(L, device);
    lua_setfield(L, LUA_REGISTRYINDEX, kRegistryKey_RenderDevice);

    if (subBackend) {
        device->setPreferredBackend(subBackend);
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_Engine_select_audio_backend(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    auto& registry = BackendRegistry::instance();
    IAudioBackend* backend = registry.createAudioBackend(name);
    if (!backend) {
        lua_pushnil(L);
        lua_pushfstring(L, "Unknown audio backend: %s", name);
        return 2;
    }

    lua_pushlightuserdata(L, backend);
    lua_setfield(L, LUA_REGISTRYINDEX, kRegistryKey_AudioBackend);

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_Engine_select_platform_backend(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    auto& registry = BackendRegistry::instance();
    IPlatformBackend* backend = registry.createPlatformBackend(name);
    if (!backend) {
        lua_pushnil(L);
        lua_pushfstring(L, "Unknown platform backend: %s", name);
        return 2;
    }

    lua_pushlightuserdata(L, backend);
    lua_setfield(L, LUA_REGISTRYINDEX, kRegistryKey_PlatformBackend);

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_Engine_get_backend_info(lua_State* L) {
    auto& registry = BackendRegistry::instance();

    lua_newtable(L);

    IRenderDevice* renderDev = registry.getRenderDevice();
    if (renderDev) {
        lua_pushstring(L, renderDev->getBackendName());
        lua_setfield(L, -2, "render");
    }

    IAudioBackend* audioBackend = registry.getAudioBackend();
    if (audioBackend) {
        lua_pushstring(L, audioBackend->getBackendName());
        lua_setfield(L, -2, "audio");
    }

    IPlatformBackend* platBackend = registry.getPlatformBackend();
    if (platBackend) {
        lua_pushstring(L, platBackend->getBackendName());
        lua_setfield(L, -2, "platform");
    }

    return 1;
}

// Internal runner bridge: publish its exact session table, or clear on stop.
// Require an explicit argument so a missing value cannot silently clear state.
static int lua_Engine_bind_active_context(lua_State* L) {
    if (lua_gettop(L) != 1) {
        return luaL_error(L,
            "Engine.bind_active_context expects exactly one table or nil");
    }
    if (!GameState::bind(L, 1)) {
        return luaL_argerror(L, 1, "table or nil expected");
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_Engine_report_command_error(lua_State* L) {
    // Script runtime error -> di ErrorReporter -> ErrorUI (installed by the
    // composition root). Accepts (command, error, scene?, line?) and returns 1
    // always; no-op when no reporter is installed (headless/tests).
    auto& registry = BackendRegistry::instance();
    const auto& reporter = registry.getErrorReporter();
    if (reporter) {
        const char* cmd   = luaL_checkstring(L, 1);
        const char* err   = luaL_checkstring(L, 2);
        const char* scene = luaL_optstring(L, 3, "");
        int line = static_cast<int>(luaL_optinteger(L, 4, 0));
        reporter(cmd ? cmd : "", err ? err : "", scene ? scene : "", line);
    }
    lua_pushboolean(L, 1);
    return 1;
}

void registerEngineBindings(lua_State* L) {
    static const luaL_Reg engine_funcs[] = {
        { "select_render_backend",   lua_Engine_select_render_backend   },
        { "select_audio_backend",    lua_Engine_select_audio_backend    },
        { "select_platform_backend", lua_Engine_select_platform_backend },
        { "get_backend_info",        lua_Engine_get_backend_info        },
        { "bind_active_context",     lua_Engine_bind_active_context     },
        { "report_command_error",      lua_Engine_report_command_error      },
        { nullptr, nullptr }
    };

    luaL_newlib(L, engine_funcs);
    lua_setglobal(L, "Engine");
    printf("[Lua] Engine (backend selection) module registered.\n");
}

} // namespace engine_binding
} // namespace Caesura
