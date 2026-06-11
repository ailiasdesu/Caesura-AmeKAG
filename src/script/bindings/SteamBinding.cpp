// SteamBinding — Lua bindings: steam.unlock_achievement, steam.set_stat, steam.cloud_*
#include "SteamBinding.h"
#include "../Steam/ISteamBackend.h"
#include <cstdio>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace Caesura {

static ISteamBackend* g_steam = nullptr;

#define STEAM_BODY(name, code) \
    static int lua_steam_##name(lua_State* L) { if (!g_steam) { lua_pushboolean(L, 0); return 1; } code }

STEAM_BODY(unlock_achievement, {
    const char* id = luaL_checkstring(L, 1);
    lua_pushboolean(L, g_steam->unlockAchievement(id) ? 1 : 0);
    return 1;
})

STEAM_BODY(is_achievement_unlocked, {
    const char* id = luaL_checkstring(L, 1);
    lua_pushboolean(L, g_steam->isAchievementUnlocked(id) ? 1 : 0);
    return 1;
})

STEAM_BODY(reset_achievement, {
    const char* id = luaL_checkstring(L, 1);
    lua_pushboolean(L, g_steam->resetAchievement(id) ? 1 : 0);
    return 1;
})

STEAM_BODY(reset_all_achievements, {
    lua_pushboolean(L, g_steam->resetAllAchievements() ? 1 : 0);
    return 1;
})

STEAM_BODY(set_stat_int, {
    const char* name = luaL_checkstring(L, 1);
    lua_Integer val = luaL_checkinteger(L, 2);
    lua_pushboolean(L, g_steam->setStatInt(name, (int32_t)val) ? 1 : 0);
    return 1;
})

STEAM_BODY(get_stat_int, {
    const char* name = luaL_checkstring(L, 1);
    lua_pushinteger(L, g_steam->getStatInt(name));
    return 1;
})

STEAM_BODY(set_stat_float, {
    const char* name = luaL_checkstring(L, 1);
    float val = (float)luaL_checknumber(L, 2);
    lua_pushboolean(L, g_steam->setStatFloat(name, val) ? 1 : 0);
    return 1;
})

STEAM_BODY(get_stat_float, {
    const char* name = luaL_checkstring(L, 1);
    lua_pushnumber(L, g_steam->getStatFloat(name));
    return 1;
})

STEAM_BODY(store_stats, {
    lua_pushboolean(L, g_steam->storeStats() ? 1 : 0);
    return 1;
})

STEAM_BODY(is_overlay_active, {
    lua_pushboolean(L, g_steam->isOverlayActive() ? 1 : 0);
    return 1;
})

#undef STEAM_BODY

static const luaL_Reg steam_functions[] = {
    {"unlock_achievement",      lua_steam_unlock_achievement},
    {"is_achievement_unlocked", lua_steam_is_achievement_unlocked},
    {"reset_achievement",       lua_steam_reset_achievement},
    {"reset_all_achievements",  lua_steam_reset_all_achievements},
    {"set_stat_int",            lua_steam_set_stat_int},
    {"get_stat_int",            lua_steam_get_stat_int},
    {"set_stat_float",          lua_steam_set_stat_float},
    {"get_stat_float",          lua_steam_get_stat_float},
    {"store_stats",             lua_steam_store_stats},
    {"is_overlay_active",       lua_steam_is_overlay_active},
    {nullptr, nullptr}
};

void registerSteamBinding(lua_State* L, ISteamBackend* backend) {
    g_steam = backend;
    luaL_newlib(L, steam_functions);
    lua_setglobal(L, "steam");
    printf("[Lua] Steam module registered (10 APIs).\n");
}

void unregisterSteamBinding(ISteamBackend* backend) {
    if (g_steam == backend) g_steam = nullptr;
}

} // namespace Caesura
