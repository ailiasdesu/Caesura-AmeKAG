extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "KAGBinding.h"
#include "../Core/BackendRegistry.h"
#include "../Core/IAudioBackend.h"
#include "../Core/InputRouter.h"
#include "../Render/IRenderDevice.h"
#include <cstdio>
#include <cstring>
#include <string>

namespace Caesura {

// -- Forward declarations --------------------------------------------------

static int lua_KAG_play_bgm(lua_State* L);
static int lua_KAG_play_voice(lua_State* L);
static int lua_KAG_play_se_3d(lua_State* L);
static int lua_KAG_play_se(lua_State* L);
static int lua_KAG_stop_bgm(lua_State* L);
static int lua_KAG_stop_voice(lua_State* L);
static int lua_KAG_stop_se(lua_State* L);
static int lua_KAG_set_global_volume(lua_State* L);
static int lua_KAG_get_global_volume(lua_State* L);
static int lua_KAG_replay_voice(lua_State* L);
static int lua_KAG_set_bus_volume(lua_State* L);
static int lua_KAG_get_bus_volume(lua_State* L);
static int lua_KAG_flush_wave_cache(lua_State* L);
static int lua_KAG_show_text(lua_State* L);
static int lua_KAG_show_image(lua_State* L);
static int lua_KAG_clear_screen(lua_State* L);
static int lua_KAG_wait_click(lua_State* L);

static int lua_KAG_render_text(lua_State* L);
static int lua_KAG_render_ruby(lua_State* L);
static int lua_KAG_clear_text(lua_State* L);
static int lua_KAG_set_font(lua_State* L);
static int lua_KAG_line_height(lua_State* L);



static int lua_KAG_set_listener(lua_State* L);
static int lua_KAG_is_voice_playing(lua_State* L);
static int lua_KAG_is_bgm_playing(lua_State* L);
static int lua_KAG_get_active_voices(lua_State* L);
static int lua_KAG_log(lua_State* L);
static int lua_KAG_clear_text_layer(lua_State* L);
static int lua_KAG_set_bgm_volume(lua_State* L);
static int lua_KAG_set_se_volume(lua_State* L);
static int lua_KAG_set_voice_volume(lua_State* L);
static int lua_KAG_audio_get_position(lua_State* L);
static int lua_KAG_audio_get_length(lua_State* L);
static int lua_KAG_audio_fade_volume(lua_State* L);

static int lua_KAG_quake(lua_State* L);

// -- Module registration --------------------------------------------------

static const luaL_Reg kag_functions[] = {
    { "play_bgm",           lua_KAG_play_bgm           },
    { "play_voice",         lua_KAG_play_voice         },
    { "play_se_3d",         lua_KAG_play_se_3d         },
    { "play_se",            lua_KAG_play_se            },
    { "stop_bgm",           lua_KAG_stop_bgm           },
    { "stop_voice",         lua_KAG_stop_voice         },
    { "stop_se",            lua_KAG_stop_se            },
    { "set_global_volume",  lua_KAG_set_global_volume  },
    { "get_global_volume",  lua_KAG_get_global_volume  },
    { "replay_voice",       lua_KAG_replay_voice       },
    { "set_bus_volume",     lua_KAG_set_bus_volume     },
    { "get_bus_volume",     lua_KAG_get_bus_volume     },
    { "flush_wave_cache",   lua_KAG_flush_wave_cache   },
    { "show_text",          lua_KAG_show_text          },
    { "render_text",        lua_KAG_render_text        },
    { "render_ruby",        lua_KAG_render_ruby        },
    { "clear_text",         lua_KAG_clear_text         },
    { "set_font",           lua_KAG_set_font           },
    { "line_height",        lua_KAG_line_height        },
    { "wait_click",         lua_KAG_wait_click         },
    { "show_image",         lua_KAG_show_image         },
    { "clear_screen",       lua_KAG_clear_screen       },
    { "set_listener",       lua_KAG_set_listener       },
    { "is_voice_playing",   lua_KAG_is_voice_playing   },
    { "is_bgm_playing",     lua_KAG_is_bgm_playing     },
    { "get_active_voices",  lua_KAG_get_active_voices  },
    { "log",                lua_KAG_log                },
    { "clear_text_layer",   lua_KAG_clear_text_layer   },
    { "set_bgm_volume",     lua_KAG_set_bgm_volume     },
    { "set_se_volume",      lua_KAG_set_se_volume      },
    { "set_voice_volume",   lua_KAG_set_voice_volume   },
    { "audio_get_position", lua_KAG_audio_get_position },
    { "audio_get_length",   lua_KAG_audio_get_length   },
    { "audio_fade_volume",  lua_KAG_audio_fade_volume  },

    { "quake",              lua_KAG_quake              },
    { nullptr, nullptr }
};

void registerKAGBinding(lua_State* L) {
    luaL_newlib(L, kag_functions);
    lua_setglobal(L, "KAG");
    printf("[Lua] KAG module registered (35 APIs, via BackendRegistry).\n");
}

// -- Helpers ---------------------------------------------------------------

static IAudioBackend* getAudio(lua_State* L) {
    return BackendRegistry::getAudioBackendFromLua(L);
}

static IRenderDevice* getRender(lua_State* L) {
    return BackendRegistry::getRenderDeviceFromLua(L);
}

// -- KAG.play_bgm(file, fadeTime) -----------------------------------------

static int lua_KAG_play_bgm(lua_State* L) {
    const char* file = luaL_checkstring(L, 1);
    float fadeTime   = (float)luaL_optnumber(L, 2, 1.0);
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushboolean(L, 0); return 1; }
    audio->playBGM(file, fadeTime);
    lua_pushboolean(L, 1);
    return 1;
}

// -- KAG.play_voice(file) -------------------------------------------------

static int lua_KAG_play_voice(lua_State* L) {
    const char* file = luaL_checkstring(L, 1);
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushboolean(L, 0); return 1; }
    audio->playVoice(file);
    lua_pushboolean(L, 1);
    return 1;
}

// -- KAG.play_se_3d(file, x, y, z) ----------------------------------------

static int lua_KAG_play_se_3d(lua_State* L) {
    const char* file = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float z = (float)luaL_optnumber(L, 4, 0.0);
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushboolean(L, 0); return 1; }
    audio->playSE3D(file, x, y, z);
    lua_pushboolean(L, 1);
    return 1;
}

// -- KAG.play_se(file) ----------------------------------------------------

static int lua_KAG_play_se(lua_State* L) {
    const char* file = luaL_checkstring(L, 1);
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushboolean(L, 0); return 1; }
    audio->playSE(file);
    lua_pushboolean(L, 1);
    return 1;
}

// -- KAG.stop_bgm(fadeTime) -----------------------------------------------

static int lua_KAG_stop_bgm(lua_State* L) {
    float fadeTime = (float)luaL_optnumber(L, 1, 1.0);
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushboolean(L, 0); return 1; }
    audio->stopBGM(fadeTime);
    lua_pushboolean(L, 1);
    return 1;
}

// -- KAG.stop_voice() -----------------------------------------------------

static int lua_KAG_stop_voice(lua_State* L) {
    IAudioBackend* audio = getAudio(L);
    if (!audio) return 0;
    audio->stopVoice();
    return 0;
}

// -- KAG.stop_se() --------------------------------------------------------
// Stops all active sound effects via the SE bus.

static int lua_KAG_stop_se(lua_State* L) {
    IAudioBackend* audio = getAudio(L);
    if (!audio) return 0;
    audio->stopSE();
    return 0;
}

// -- KAG.set_global_volume(volume) ----------------------------------------
// Sets the master volume for all audio buses.

static int lua_KAG_set_global_volume(lua_State* L) {
    float volume = (float)luaL_checknumber(L, 1);
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushboolean(L, 0); return 1; }
    audio->setGlobalVolume(volume);
    lua_pushboolean(L, 1);
    return 1;
}

// -- KAG.get_global_volume() -> float -------------------------------------
// Returns the current master volume.

static int lua_KAG_get_global_volume(lua_State* L) {
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushnumber(L, 1.0); return 1; }
    lua_pushnumber(L, audio->getGlobalVolume());
    return 1;
}

// -- KAG.replay_voice(file) -- replay voice from backlog -------------------

static int lua_KAG_replay_voice(lua_State* L) {
    const char* file = luaL_checkstring(L, 1);
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushboolean(L, 0); return 1; }
    audio->playVoice(file);
    lua_pushboolean(L, 1);
    return 1;
}

// -- KAG.set_bus_volume(bus, volume) -- per-bus volume ---------------------

static int lua_KAG_set_bus_volume(lua_State* L) {
    const char* bus    = luaL_checkstring(L, 1);
    float volume       = (float)luaL_checknumber(L, 2);
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushboolean(L, 0); return 1; }
    audio->setBusVolume(bus, volume);
    lua_pushboolean(L, 1);
    return 1;
}

// -- KAG.get_bus_volume(bus) -> float --------------------------------------

static int lua_KAG_get_bus_volume(lua_State* L) {
    const char* bus    = luaL_checkstring(L, 1);
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushnumber(L, 1.0); return 1; }
    lua_pushnumber(L, audio->getBusVolume(bus));
    return 1;
}

// -- KAG.flush_wave_cache() -- clear C++ wave cache ------------------------

static int lua_KAG_flush_wave_cache(lua_State* L) {
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushboolean(L, 0); return 1; }
    audio->flushWaveCache();
    lua_pushboolean(L, 1);
    return 1;
}

// -- KAG.show_text(text) -- console echo -----------------------------------

static int lua_KAG_show_text(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    printf("[KAG] %s\n", text);
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_KAG_render_text(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    float x    = (float)luaL_optnumber(L, 2, 32.0);
    float y    = (float)luaL_optnumber(L, 3, 48.0);
    uint8_t r  = (uint8_t)luaL_optinteger(L, 4, 255);
    uint8_t g  = (uint8_t)luaL_optinteger(L, 5, 255);
    uint8_t b  = (uint8_t)luaL_optinteger(L, 6, 255);
    uint8_t a  = (uint8_t)luaL_optinteger(L, 7, 255);

    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }
    dev->renderText(VIEW_MAIN, text, x, y, r, g, b, a);
    lua_pushboolean(L, 1);
    return 1;
}

// -- KAG.render_ruby(text, ruby, x, y, r, g, b, a) ------------------------

static int lua_KAG_render_ruby(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    const char* ruby = luaL_checkstring(L, 2);
    float x    = (float)luaL_optnumber(L, 3, 32.0);
    float y    = (float)luaL_optnumber(L, 4, 48.0);
    uint8_t r  = (uint8_t)luaL_optinteger(L, 5, 255);
    uint8_t g  = (uint8_t)luaL_optinteger(L, 6, 255);
    uint8_t b  = (uint8_t)luaL_optinteger(L, 7, 255);
    uint8_t a  = (uint8_t)luaL_optinteger(L, 8, 255);

    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }
    dev->renderRuby(VIEW_MAIN, text, ruby, x, y, r, g, b, a);
    lua_pushboolean(L, 1);
    return 1;
}

// -- KAG.clear_text() -- reset text cursor ---------------------------------

static int lua_KAG_clear_text(lua_State* L) {
    (void)L;
    // Cursor reset is handled by TextRenderer::clearText()
    return 0;
}

// -- KAG.set_font(id) -- 0=Small 1=Large -----------------------------------

static int lua_KAG_set_font(lua_State* L) {
    int fontId = (int)luaL_optinteger(L, 1, 0);
    IRenderDevice* dev = getRender(L);
    if (dev) dev->setFont(fontId);
    lua_pushboolean(L, 1);
    return 1;
}

// -- KAG.line_height() -> float --------------------------------------------

static int lua_KAG_line_height(lua_State* L) {
    IRenderDevice* dev = getRender(L);
    float lh = dev ? dev->textLineHeight() : 16.0f;
    lua_pushnumber(L, (lua_Number)lh);
    return 1;
}

// -- KAG.wait_click() -- coroutine yield -----------------------------------

static int lua_KAG_wait_click(lua_State* L) {
    // Stub: controlled by engine input loop, not Lua
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_KAG_show_image(lua_State* L) {
    const char* file = luaL_checkstring(L, 1);
    printf("[KAG] show_image: %s\n", file);
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_KAG_clear_screen(lua_State* L) {
    printf("[KAG] clear_screen\n");
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_KAG_set_listener(lua_State* L) {
    float px = (float)luaL_checknumber(L, 1);
    float py = (float)luaL_checknumber(L, 2);
    float pz = (float)luaL_checknumber(L, 3);
    float ax = (float)luaL_checknumber(L, 4);
    float ay = (float)luaL_checknumber(L, 5);
    float az = (float)luaL_checknumber(L, 6);
    float ux = (float)luaL_optnumber(L, 7, 0.0);
    float uy = (float)luaL_optnumber(L, 8, 1.0);
    float uz = (float)luaL_optnumber(L, 9, 0.0);
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushboolean(L, 0); return 1; }
    audio->update3dListener(px, py, pz, ax, ay, az, ux, uy, uz);
    lua_pushboolean(L, 1);
    return 1;
}

// -- State queries ---------------------------------------------------------

static int lua_KAG_is_voice_playing(lua_State* L) {
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, audio->isVoicePlaying() ? 1 : 0);
    return 1;
}

static int lua_KAG_is_bgm_playing(lua_State* L) {
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, audio->isBGMPlaying() ? 1 : 0);
    return 1;
}

static int lua_KAG_get_active_voices(lua_State* L) {
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, audio->activeVoiceCount());
    return 1;
}

static int lua_KAG_log(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    printf("[KAG:LOG] %s\n", msg);
    return 0;
}


// -- KAG.clear_text_layer() -- alias for clear_text -----------------------

static int lua_KAG_clear_text_layer(lua_State* L) {
    return lua_KAG_clear_text(L);
}

// -- KAG.set_bgm_volume(vol) ----------------------------------------------

static int lua_KAG_set_bgm_volume(lua_State* L) {
    float vol = (float)luaL_checknumber(L, 1);
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushboolean(L, 0); return 1; }
    audio->setBusVolume("bgm", vol);
    lua_pushboolean(L, 1);
    return 1;
}

// -- KAG.set_se_volume(vol) -----------------------------------------------

static int lua_KAG_set_se_volume(lua_State* L) {
    float vol = (float)luaL_checknumber(L, 1);
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushboolean(L, 0); return 1; }
    audio->setBusVolume("se", vol);
    lua_pushboolean(L, 1);
    return 1;
}

// -- KAG.set_voice_volume(vol) --------------------------------------------

static int lua_KAG_set_voice_volume(lua_State* L) {
    float vol = (float)luaL_checknumber(L, 1);
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushboolean(L, 0); return 1; }
    audio->setBusVolume("voice", vol);
    lua_pushboolean(L, 1);
    return 1;
}

// -- KAG.quake(time_ms, intensity) ----------------------------------------

static int lua_KAG_quake(lua_State* L) {
    float timeMs    = (float)luaL_checknumber(L, 1);
    float intensity = (float)luaL_optnumber(L, 2, 5.0);
    // Quake parameters stored in Lua registry for the VFX layer to consume
    // The actual screen-shake is applied per-frame by vfx.lua
    lua_pushnumber(L, intensity);
    lua_setfield(L, LUA_REGISTRYINDEX, "Caesura.QuakeIntensity");
    lua_pushnumber(L, timeMs);
    lua_setfield(L, LUA_REGISTRYINDEX, "Caesura.QuakeDuration");
    printf("[KAG] Quake: %.0f ms, intensity %.1f\n", timeMs, intensity);
    lua_pushboolean(L, 1);
    return 1;
}
// -- KAG.audio_get_position(bus) -- Spec [3.3] ---------------------------

static int lua_KAG_audio_get_position(lua_State* L) {
    const char* bus = luaL_checkstring(L, 1);
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushnumber(L, 0.0); return 1; }
    lua_pushnumber(L, (lua_Number)audio->getPosition(bus));
    return 1;
}

// -- KAG.audio_get_length(bus) -- Spec [3.3] -----------------------------

static int lua_KAG_audio_get_length(lua_State* L) {
    const char* bus = luaL_checkstring(L, 1);
    IAudioBackend* audio = getAudio(L);
    if (!audio) { lua_pushnumber(L, 0.0); return 1; }
    lua_pushnumber(L, (lua_Number)audio->getLength(bus));
    return 1;
}

// -- KAG.audio_fade_volume(bus, target_vol, fade_time) -- Spec [3.2] ----

static int lua_KAG_audio_fade_volume(lua_State* L) {
    const char* bus       = luaL_checkstring(L, 1);
    float targetVolume    = (float)luaL_checknumber(L, 2);
    float fadeTime        = (float)luaL_checknumber(L, 3);
    IAudioBackend* audio  = getAudio(L);
    if (!audio) { lua_pushboolean(L, 0); return 1; }
    audio->fadeVolume(bus, targetVolume, fadeTime);
    lua_pushboolean(L, 1);
    return 1;
}

} // namespace Caesura