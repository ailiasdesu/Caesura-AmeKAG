extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "UnifiedBinding.h"
#include "../Core/BackendRegistry.h"
#include "../Core/IAudioBackend.h"
#include "../Scripting/VFXBinding.h"
#include "../Resource/AsyncLoader.h"
#include <cstdio>
#include <cstring>

namespace Caesura {

// =========================================================================
//  Unified _CAESURA_BACKEND Lua proxy — spec [0.4]
//  ALL commands delegate to existing Render / KAG / DevCore globals.
//  No duplicated logic; single source of truth.
// =========================================================================

// -- Helpers: call global.func(args...) and return results ---------------

static int delegateToTable(lua_State* L, const char* tableName, const char* funcName, int nargs) {
    lua_getglobal(L, tableName);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_pushfstring(L, "Global table '%s' not found", tableName);
        return 2;
    }
    lua_getfield(L, -1, funcName);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        lua_pushnil(L);
        lua_pushfstring(L, "Function '%s.%s' not found", tableName, funcName);
        return 2;
    }
    lua_insert(L, -1 - nargs);
    lua_pop(L, 1);
    if (lua_pcall(L, nargs, LUA_MULTRET, 0) != LUA_OK) {
        lua_pushnil(L);
        lua_pushfstring(L, "%s.%s error: %s", tableName, funcName, lua_tostring(L, -1));
        return 2;
    }
    return lua_gettop(L);
}

static int delegateToGlobalFunc(lua_State* L, const char* tableName, const char* funcName) {
    int nargs = lua_gettop(L) - 1;
    lua_remove(L, 1);
    return delegateToTable(L, tableName, funcName, nargs);
}

// =========================================================================
//  b:render(cmd, ...) — render → Render, particles → VFX
// =========================================================================

static int lua_Backend_render(lua_State* L) {
    const char* cmd = luaL_checkstring(L, 1);
    // -- Render table commands
    static const struct { const char* cmd; const char* func; } map[] = {
        {"create_viewport","create_viewport"}, {"destroy_viewport","destroy_viewport"},
        {"draw_viewport","draw_viewport"}, {"fill_viewport","fill_viewport"},
        {"load_texture","load_texture"}, {"destroy_texture","destroy_texture"},
        {"create_solid_texture","create_solid_texture"}, {"free_texture","free_texture"},
        {"get_resolution","get_resolution"}, {"set_view_name","set_view_name"},
        {"submit_batch","submit_batch"}, {"submit_blend","submit_blend"},
        {"submit_transition","submit_transition"}, {"submit_vfx","submit_vfx"},
        {"submit_stretch_blt","submit_stretch_blt"}, {"submit_affine_blt","submit_affine_blt"},
        {"clear_text","clear_text"}, {"render_ruby","render_ruby"},
        {"set_font","set_font"}, {"line_height","line_height"},
        {"begin_batch","begin_batch"}, {"flush_batch","flush_batch"},
        {"resize","resize"}, {"load_texture_async","load_texture_async"}, {"cancel_async_loads","cancel_async_loads"}, {nullptr,nullptr}
    };
    for (auto* m = map; m->cmd; m++) {
        if (strcmp(cmd, m->cmd) == 0) {
            lua_remove(L, 1);
            return delegateToGlobalFunc(L, "Render", m->func);
        }
    }
    // -- VFX table commands (particles)
    static const struct { const char* cmd; const char* func; } vfxMap[] = {
        {"particles_init","particles_init"},{"particles_shutdown","particles_shutdown"},
        {"particles_create_emitter","particles_create_emitter"},
        {"particles_destroy_emitter","particles_destroy_emitter"},
        {"particles_emit","particles_emit"},{"particles_update","particles_update"},
        {"particles_render","particles_render"},{"particles_alive_count","particles_alive_count"},
        {"particles_clear","particles_clear"},{"begin_particles","begin_particles"},
        {nullptr,nullptr}
    };
    for (auto* m = vfxMap; m->cmd; m++) {
        if (strcmp(cmd, m->cmd) == 0) {
            lua_remove(L, 1);
            return delegateToGlobalFunc(L, "VFX", m->func);
        }
    }
    lua_pushnil(L);
    lua_pushfstring(L, "Unknown render command: %s", cmd);
    return 2;
}

// =========================================================================
//  b:audio(cmd, ...) — ALL audio commands delegate to KAG table
//  Fix #9: is_playing supports bus-dependent routing
// =========================================================================

static int lua_Backend_audio(lua_State* L) {
    const char* cmd = luaL_checkstring(L, 1);
    IAudioBackend* audio = BackendRegistry::getAudioBackendFromLua(L);

    // #9: is_playing with bus routing
    if (strcmp(cmd, "is_playing") == 0) {
        const char* bus = luaL_optstring(L, 2, "voice");
        if (strcmp(bus, "voice") == 0) {
            lua_remove(L, 1); lua_remove(L, 1);
            return delegateToGlobalFunc(L, "KAG", "is_voice_playing");
        } else if (strcmp(bus, "bgm") == 0) {
            lua_remove(L, 1); lua_remove(L, 1);
            return delegateToGlobalFunc(L, "KAG", "is_bgm_playing");
        } else if (strcmp(bus, "se") == 0) {
            int count = 0;
            IAudioBackend* audio = BackendRegistry::getAudioBackendFromLua(L);
            if (audio) count = audio->activeVoiceCount();
            lua_pushboolean(L, count > 0 ? 1 : 0);
            return 1;
        }
        lua_pushboolean(L, 0);
        return 1;
    }

    // fade_volume -> KAG.audio_fade_volume (Spec [3.2])
    if (strcmp(cmd, "fade_volume") == 0) {
        const char* bus = luaL_checkstring(L, 2);
        float target = (float)luaL_checknumber(L, 3);
        float time   = (float)luaL_checknumber(L, 4);
        if (audio) audio->fadeVolume(bus, target, time);
        printf("[Backend] Fade %s volume to %.2f over %.1fs\n", bus, target, time);
        lua_pushboolean(L, 1);
        return 1;
    }

    // xfade -> stop_bgm + play_bgm
    if (strcmp(cmd, "xfade") == 0) {
        const char* file = luaL_checkstring(L, 3);
        float fadeTime = (float)luaL_optnumber(L, 4, 1.0);
        lua_getglobal(L, "KAG"); lua_getfield(L, -1, "stop_bgm");
        lua_pushnumber(L, fadeTime); lua_pcall(L, 1, 0, 0);
        lua_getglobal(L, "KAG"); lua_getfield(L, -1, "play_bgm");
        lua_pushstring(L, file); lua_pushnumber(L, fadeTime);
        lua_pcall(L, 2, 1, 0);
        return 1;
    }

    // get_length / get_position → IAudioBackend (Spec [3.3])
    if (strcmp(cmd, "get_length") == 0) {
        const char* bus = luaL_checkstring(L, 2);
        float len = audio ? audio->getLength(bus) : 0.0f;
        lua_pushnumber(L, (lua_Number)len);
        return 1;
    }
    if (strcmp(cmd, "get_position") == 0) {
        const char* bus = luaL_checkstring(L, 2);
        float pos = audio ? audio->getPosition(bus) : 0.0f;
        lua_pushnumber(L, (lua_Number)pos);
        return 1;
    }

    // Generic audio -> KAG.func
    static const struct { const char* cmd; const char* func; } map[] = {
        {"play_bgm","play_bgm"},{"play_voice","play_voice"},
        {"play_se_3d","play_se_3d"},{"play_se","play_se"},
        {"stop_bgm","stop_bgm"},{"stop_voice","stop_voice"},{"stop_se","stop_se"},
        {"set_listener","set_listener"},{"set_bus_volume","set_bus_volume"},
        {"get_bus_volume","get_bus_volume"},{"set_global_volume","set_global_volume"},
        {"get_global_volume","get_global_volume"},{"flush_wave_cache","flush_wave_cache"},
        {"is_voice_playing","is_voice_playing"},{"is_bgm_playing","is_bgm_playing"},
        {"get_active_voices","get_active_voices"}, {nullptr,nullptr}
    };
    for (auto* m = map; m->cmd; m++) {
        if (strcmp(cmd, m->cmd) == 0) {
            lua_remove(L, 1);
            return delegateToGlobalFunc(L, "KAG", m->func);
        }
    }
    lua_pushnil(L);
    lua_pushfstring(L, "Unknown audio command: %s", cmd);
    return 2;
}

// =========================================================================
//  b:platform(cmd, ...) — ALL platform commands delegate to DevCore
// =========================================================================

static int lua_Backend_platform(lua_State* L) {
    const char* cmd = luaL_checkstring(L, 1);
    static const struct { const char* cmd; const char* func; } map[] = {
        {"log","log"},{"get_resolution","get_resolution"},
        {"set_input_focus","set_input_focus"},{"get_input_focus","get_input_focus"},
        {"set_fullscreen","set_fullscreen"},{nullptr,nullptr}
    };
    for (auto* m = map; m->cmd; m++) {
        if (strcmp(cmd, m->cmd) == 0) {
            lua_remove(L, 1);
            return delegateToGlobalFunc(L, "DevCore", m->func);
        }
    }
    lua_pushnil(L);
    lua_pushfstring(L, "Unknown platform command: %s", cmd);
    return 2;
}

// =========================================================================
//  UI convenience — delegate to KAG
// =========================================================================

static int lua_Backend_show_text(lua_State* L)   { return delegateToGlobalFunc(L, "KAG", "show_text"); }
static int lua_Backend_show_image(lua_State* L)  { return delegateToGlobalFunc(L, "KAG", "show_image"); }
static int lua_Backend_clear_screen(lua_State* L){ return delegateToGlobalFunc(L, "KAG", "clear_screen"); }
static int lua_Backend_wait_click(lua_State* L)  { return delegateToGlobalFunc(L, "KAG", "wait_click"); }


// =========================================================================
//  b:render("load_texture_async", path, callback_fn) ? async load
// =========================================================================

static int lua_Backend_load_texture_async(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    if (!lua_isfunction(L, 2)) {
        lua_pushnil(L);
        lua_pushstring(L, "load_texture_async: second argument must be a callback function");
        return 2;
    }

    // Store callback in registry so it survives across yields
    lua_pushvalue(L, 2);  // copy callback
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);

    int reqId = AsyncLoader::instance().enqueue(path, "texture");
    if (reqId < 0) {
        luaL_unref(L, LUA_REGISTRYINDEX, cbRef);
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Queue full (max 16 pending)");
        return 2;
    }

    // Store (reqId, cbRef) mapping in a global weak table
    lua_getglobal(L, "_ASYNC_CALLBACKS");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "_ASYNC_CALLBACKS");
    }
    lua_pushinteger(L, reqId);
    lua_pushinteger(L, cbRef);
    lua_settable(L, -3);
    lua_pop(L, 1);

    lua_pushinteger(L, reqId);
    return 1;
}

static int lua_Backend_cancel_async_loads(lua_State* L) {
    AsyncLoader::instance().cancelAll();
    // Release all stored callbacks
    lua_getglobal(L, "_ASYNC_CALLBACKS");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            int cbRef = (int)lua_tointeger(L, -1);
            luaL_unref(L, LUA_REGISTRYINDEX, cbRef);
            lua_pop(L, 1);
        }
        lua_newtable(L);
        lua_setglobal(L, "_ASYNC_CALLBACKS");
    }
    lua_pop(L, 1);
    lua_pushboolean(L, 1);
    return 1;
}

// =========================================================================
//  Module registration
// =========================================================================

static const luaL_Reg backend_methods[] = {
    { "audio",       lua_Backend_audio       },
    { "render",      lua_Backend_render      },
    { "platform",    lua_Backend_platform    },
    { "show_text",   lua_Backend_show_text   },
    { "show_image",  lua_Backend_show_image  },
    { "clear_screen", lua_Backend_clear_screen },
    { "wait_click",  lua_Backend_wait_click  },
    { nullptr, nullptr }
};

void registerUnifiedBackendBinding(lua_State* L) {
    lua_newtable(L);
    luaL_setfuncs(L, backend_methods, 0);
    lua_setglobal(L, "_CAESURA_BACKEND");
    printf("[Lua] Unified _CAESURA_BACKEND proxy registered (delegates to Render/KAG/DevCore).\n");
}

} // namespace Caesura