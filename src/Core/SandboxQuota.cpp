// ===========================================================================
//  Caesura (AmeKAG) -- SandboxQuota implementation (Track 3)
// ===========================================================================

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "SandboxQuota.h"
#include <cstdio>
#include <string>

namespace Caesura {
namespace SandboxQuota {

// ---------------------------------------------------------------------------
// Internal helper: push _SANDBOX_RESOURCES table, return true if valid
// ---------------------------------------------------------------------------
static bool getResourcesTable(lua_State* L) {
    lua_getglobal(L, "_SANDBOX_RESOURCES");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return false;  // sandbox not loaded yet -- allow everything
    }
    return true;
}

// ---------------------------------------------------------------------------
// tryAlloc -- increment counter, fail if at limit
// ---------------------------------------------------------------------------
bool tryAlloc(lua_State* L, const char* kind) {
    if (!L) return true;  // no Lua state yet -- allow

    if (!getResourcesTable(L)) return true;

    std::string loadedKey = std::string(kind) + "_loaded";
    std::string maxKey    = std::string(kind) + "_max";

    // Read max
    lua_getfield(L, -1, maxKey.c_str());
    int max = lua_isnumber(L, -1) ? (int)lua_tonumber(L, -1) : 999999;
    lua_pop(L, 1);

    // Read current
    lua_getfield(L, -1, loadedKey.c_str());
    int cur = lua_isnumber(L, -1) ? (int)lua_tonumber(L, -1) : 0;
    lua_pop(L, 1);

    if (cur >= max) {
        lua_pop(L, 1);  // pop resources table
        fprintf(stderr, "[Sandbox] %s quota exceeded (%d/%d)\n", kind, cur, max);
        return false;
    }

    // Increment
    lua_pushinteger(L, cur + 1);
    lua_setfield(L, -2, loadedKey.c_str());
    lua_pop(L, 1);

    printf("[Sandbox] %s: %d/%d\n", kind, cur + 1, max);
    return true;
}

// ---------------------------------------------------------------------------
// release -- decrement counter (floor at 0)
// ---------------------------------------------------------------------------
void release(lua_State* L, const char* kind) {
    if (!L) return;

    if (!getResourcesTable(L)) return;

    std::string loadedKey = std::string(kind) + "_loaded";

    lua_getfield(L, -1, loadedKey.c_str());
    int cur = lua_isnumber(L, -1) ? (int)lua_tonumber(L, -1) : 0;
    lua_pop(L, 1);

    if (cur > 0) {
        lua_pushinteger(L, cur - 1);
        lua_setfield(L, -2, loadedKey.c_str());
    }
    lua_pop(L, 1);
}

// ---------------------------------------------------------------------------
// count -- read current value
// ---------------------------------------------------------------------------
int count(lua_State* L, const char* kind) {
    if (!L) return 0;
    if (!getResourcesTable(L)) return 0;
    std::string loadedKey = std::string(kind) + "_loaded";
    lua_getfield(L, -1, loadedKey.c_str());
    int c = lua_isnumber(L, -1) ? (int)lua_tonumber(L, -1) : 0;
    lua_pop(L, 2);  // value + table
    return c;
}

// ---------------------------------------------------------------------------
// maxLimit -- read max value
// ---------------------------------------------------------------------------
int maxLimit(lua_State* L, const char* kind) {
    if (!L) return 0;
    if (!getResourcesTable(L)) return 0;
    std::string maxKey = std::string(kind) + "_max";
    lua_getfield(L, -1, maxKey.c_str());
    int m = lua_isnumber(L, -1) ? (int)lua_tonumber(L, -1) : 0;
    lua_pop(L, 2);
    return m;
}

} // namespace SandboxQuota
} // namespace Caesura
