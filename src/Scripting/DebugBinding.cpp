extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "DebugBinding.h"
#include "../di/BackendRegistry.h"
#include <cstdio>
#include <string>

namespace Caesura {

// -- Forward declarations ---------------------------------------------------

static int lua_Debug_get_last_error(lua_State* L);
static int lua_Debug_get_error_count(lua_State* L);
static int lua_Debug_get_subsystem_stats(lua_State* L);
static int lua_Debug_dump_report(lua_State* L);
static int lua_Debug_get_render_info(lua_State* L);
static int lua_Debug_get_audio_info(lua_State* L);
static int lua_Debug_get_input_info(lua_State* L);
static int lua_Debug_get_log_path(lua_State* L);
static int lua_Debug_log(lua_State* L);
static int lua_Debug_get_stats(lua_State* L);

// -- Module registration ---------------------------------------------------

static const luaL_Reg debug_functions[] = {
    { "get_last_error",        lua_Debug_get_last_error        },
    { "get_error_count",       lua_Debug_get_error_count       },
    { "get_subsystem_stats",   lua_Debug_get_subsystem_stats   },
    { "dump_report",           lua_Debug_dump_report           },
    { "get_render_info",       lua_Debug_get_render_info       },
    { "get_audio_info",        lua_Debug_get_audio_info        },
    { "get_input_info",        lua_Debug_get_input_info        },
    { "get_log_path",          lua_Debug_get_log_path          },
    { "log",                   lua_Debug_log                   },
    { "get_stats",             lua_Debug_get_stats             },
    { nullptr, nullptr }
};

void registerDebugBinding(lua_State* L) {
    luaL_newlib(L, debug_functions);
    lua_setglobal(L, "Debug");
    printf("[Lua] Debug module registered (10 APIs).\n");
}

// -- Helpers ----------------------------------------------------------------

static void pushErrorEntry(lua_State* L, const LogEntry* entry) {
    if (!entry) {
        lua_pushnil(L);
        return;
    }
    lua_newtable(L);
    lua_pushstring(L, entry->message.c_str());
    lua_setfield(L, -2, "message");
    lua_pushinteger(L, static_cast<lua_Integer>(entry->errorCode));
    lua_setfield(L, -2, "code");
    lua_pushstring(L, ErrCodeName(entry->errorCode));
    lua_setfield(L, -2, "code_name");
    lua_pushstring(L, SubSysName(entry->subsystem));
    lua_setfield(L, -2, "subsystem");
    lua_pushstring(L, DbgLevelName(entry->level));
    lua_setfield(L, -2, "level");
}

// -- Debug.get_last_error() --

static int lua_Debug_get_last_error(lua_State* L) {
    auto& dm = (*BackendRegistry::instance().getDebugManager());
    pushErrorEntry(L, dm.lastError());
    return 1;
}

// -- Debug.get_error_count() --

static int lua_Debug_get_error_count(lua_State* L) {
    auto& dm = (*BackendRegistry::instance().getDebugManager());
    lua_pushinteger(L, dm.errorCount());
    return 1;
}

// -- Debug.get_subsystem_stats(name) --

static int lua_Debug_get_subsystem_stats(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    SubSys ss = SubSys::Dbg;
    std::string s(name);
    if (s == "render")    ss = SubSys::Render;
    if (s == "audio")     ss = SubSys::Audio;
    if (s == "scripting") ss = SubSys::Scripting;
    if (s == "input")     ss = SubSys::Input;
    if (s == "platform")  ss = SubSys::Platform;
    if (s == "engine")    ss = SubSys::Engine;
    auto& dm = (*BackendRegistry::instance().getDebugManager());
    auto st  = dm.getSubsystemStats(ss);
    lua_newtable(L);
    lua_pushinteger(L, st.totalCalls);  lua_setfield(L, -2, "total_calls");
    lua_pushinteger(L, st.errorCount);  lua_setfield(L, -2, "errors");
    lua_pushinteger(L, st.warnCount);   lua_setfield(L, -2, "warns");
    lua_pushinteger(L, st.lastErrorCode);lua_setfield(L, -2, "last_error_code");
    lua_pushstring(L, st.lastErrorMessage.c_str()); lua_setfield(L, -2, "last_error_msg");
    return 1;
}

// -- Debug.dump_report() --

static int lua_Debug_dump_report(lua_State* L) {
    auto& dm = (*BackendRegistry::instance().getDebugManager());
    std::string json = dm.dumpFullReport();
    lua_pushstring(L, json.c_str());
    return 1;
}

// -- Debug.get_render_info() --

static int lua_Debug_get_render_info(lua_State* L) {
    auto& dm = (*BackendRegistry::instance().getDebugManager());
    auto ri  = dm.getRenderInfo();
    lua_newtable(L);
    lua_pushstring(L, ri.backendName.c_str()); lua_setfield(L, -2, "backend");
    lua_pushinteger(L, ri.width);              lua_setfield(L, -2, "width");
    lua_pushinteger(L, ri.height);             lua_setfield(L, -2, "height");
    lua_pushinteger(L, ri.viewCount);          lua_setfield(L, -2, "views");
    lua_pushinteger(L, ri.textureCount);       lua_setfield(L, -2, "textures");
    lua_pushinteger(L, ri.rttCount);           lua_setfield(L, -2, "rtts");
    lua_pushboolean(L, ri.shaderReady ? 1 : 0);lua_setfield(L, -2, "shader_ready");
    return 1;
}

// -- Debug.get_audio_info() --

static int lua_Debug_get_audio_info(lua_State* L) {
    auto& dm = (*BackendRegistry::instance().getDebugManager());
    auto ai  = dm.getAudioInfo();
    lua_newtable(L);
    lua_pushboolean(L, ai.initialized ? 1 : 0); lua_setfield(L, -2, "initialized");
    lua_pushinteger(L, ai.activeVoices);         lua_setfield(L, -2, "active_voices");
    lua_pushinteger(L, ai.activeBGM);            lua_setfield(L, -2, "active_bgm");
    lua_pushnumber(L, ai.globalVolume);          lua_setfield(L, -2, "global_volume");
    lua_pushboolean(L, ai.bgmBusReady ? 1 : 0);  lua_setfield(L, -2, "bgm_bus_ready");
    lua_pushboolean(L, ai.voiceBusReady ? 1 : 0);lua_setfield(L, -2, "voice_bus_ready");
    lua_pushboolean(L, ai.seBusReady ? 1 : 0);   lua_setfield(L, -2, "se_bus_ready");
    return 1;
}

// -- Debug.get_input_info() --

static int lua_Debug_get_input_info(lua_State* L) {
    auto& dm = (*BackendRegistry::instance().getDebugManager());
    auto ii  = dm.getInputInfo();
    lua_newtable(L);
    lua_pushstring(L, ii.currentFocus.c_str());        lua_setfield(L, -2, "focus");
    lua_pushinteger(L, ii.kagCallbackCount);            lua_setfield(L, -2, "kag_callbacks");
    lua_pushinteger(L, ii.gameCallbackCount);           lua_setfield(L, -2, "game_callbacks");
    lua_pushboolean(L, ii.clickPending ? 1 : 0);        lua_setfield(L, -2, "click_pending");
    return 1;
}

// -- Debug.get_log_path() --

static int lua_Debug_get_log_path(lua_State* L) {
    auto& dm = (*BackendRegistry::instance().getDebugManager());
    lua_pushstring(L, dm.logFilePath().c_str());
    return 1;
}


// -- Debug.log(level, msg) ------------------------------------------------

static int lua_Debug_log(lua_State* L) {
    const char* level = luaL_checkstring(L, 1);
    const char* msg   = luaL_checkstring(L, 2);
    auto& dm = (*BackendRegistry::instance().getDebugManager());
    std::string lvl(level);
    if (lvl == "info")           dm.log(DbgLevel::Info, SubSys::Dbg, "%s", msg);
    else if (lvl == "warn" || lvl == "warning") dm.log(DbgLevel::Warn, SubSys::Dbg, "%s", msg);
    else if (lvl == "error")     dm.log(DbgLevel::Err, SubSys::Dbg, "%s", msg);
    else                         dm.log(DbgLevel::Info, SubSys::Dbg, "%s", msg);
    printf("[Debug:%s] %s\n", level, msg);
    return 0;
}

// -- Debug.get_stats() -- aggregate stats from all subsystems -------------

static int lua_Debug_get_stats(lua_State* L) {
    auto& dm = (*BackendRegistry::instance().getDebugManager());
    lua_newtable(L);

    // Error stats
    lua_pushinteger(L, dm.errorCount());
    lua_setfield(L, -2, "error_count");

    // Render info
    auto ri = dm.getRenderInfo();
    lua_newtable(L);
    lua_pushstring(L, ri.backendName.c_str()); lua_setfield(L, -2, "backend");
    lua_pushinteger(L, ri.width);              lua_setfield(L, -2, "width");
    lua_pushinteger(L, ri.height);             lua_setfield(L, -2, "height");
    lua_pushinteger(L, ri.viewCount);          lua_setfield(L, -2, "views");
    lua_pushinteger(L, ri.textureCount);       lua_setfield(L, -2, "textures");
    lua_pushinteger(L, ri.rttCount);           lua_setfield(L, -2, "rtts");
    lua_setfield(L, -2, "render");

    // Audio info
    auto ai = dm.getAudioInfo();
    lua_newtable(L);
    lua_pushboolean(L, ai.initialized ? 1 : 0); lua_setfield(L, -2, "initialized");
    lua_pushinteger(L, ai.activeVoices);         lua_setfield(L, -2, "active_voices");
    lua_pushnumber(L, ai.globalVolume);          lua_setfield(L, -2, "global_volume");
    lua_setfield(L, -2, "audio");

    // Input info
    auto ii = dm.getInputInfo();
    lua_newtable(L);
    lua_pushstring(L, ii.currentFocus.c_str());  lua_setfield(L, -2, "focus");
    lua_pushboolean(L, ii.clickPending ? 1 : 0); lua_setfield(L, -2, "click_pending");
    lua_setfield(L, -2, "input");

    return 1;
}} // namespace Caesura
