// ===========================================================================
//  Caesura (AmeKAG) -- SandboxQuota implementation (Track 3)
// ===========================================================================

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "SandboxQuota.h"
#include <cstdio>
#include <stdexcept>
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

    // Keep the key in Lua storage: a protected Lua error must not jump over
    // a live std::string destructor in callers such as restore cleanup.
    lua_pushfstring(L, "%s_loaded", kind);
    lua_gettable(L, -2);
    int cur = lua_isnumber(L, -1) ? (int)lua_tonumber(L, -1) : 0;
    lua_pop(L, 1);

    if (cur > 0) {
        lua_pushfstring(L, "%s_loaded", kind);
        lua_pushinteger(L, cur - 1);
        lua_settable(L, -3);
    }
    lua_pop(L, 1);
}

// ---------------------------------------------------------------------------
// count -- read current value
// ---------------------------------------------------------------------------
int count(lua_State* L, const char* kind) {
    if (!L) return 0;
    if (!getResourcesTable(L)) return 0;
    lua_pushfstring(L, "%s_loaded", kind);
    lua_gettable(L, -2);
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

namespace {
int protectedRelease(lua_State* state) {
    SandboxQuota::release(state, static_cast<const char*>(lua_touserdata(state, 1)));
    return 0;
}

int protectedCount(lua_State* state) {
    const int result = SandboxQuota::count(
        state, static_cast<const char*>(lua_touserdata(state, 1)));
    lua_pushinteger(state, result);
    return 1;
}

// The service owns a specific Lua thread, usually the main VM. A caller's
// coroutine pcall cannot catch an error raised on that different thread.
int callProtectedQuota(lua_State* state, const char* kind,
                       lua_CFunction action, int resultCount) {
    if (!state) return 0;
    const int stackTop = lua_gettop(state);
    if (!lua_checkstack(state, 2))
        throw std::runtime_error("Cannot grow sandbox quota stack");
    lua_pushcfunction(state, action);
    lua_pushlightuserdata(state, const_cast<char*>(kind));
    if (lua_pcall(state, 1, resultCount, 0) != LUA_OK) {
        char error[256] = {};
        const char* message = lua_type(state, -1) == LUA_TSTRING
            ? lua_tostring(state, -1) : nullptr;
        std::snprintf(error, sizeof(error), "%s",
            message && message[0] ? message : "Sandbox quota operation failed");
        lua_settop(state, stackTop);
        throw std::runtime_error(error);
    }
    const int result = resultCount ? static_cast<int>(lua_tointeger(state, -1)) : 0;
    lua_settop(state, stackTop);
    return result;
}
} // namespace

bool SandboxQuotaService::tryAlloc(const char* kind) {
    return SandboxQuota::tryAlloc(m_L, kind);
}

void SandboxQuotaService::release(const char* kind) {
    callProtectedQuota(m_L, kind, protectedRelease, 0);
}

int SandboxQuotaService::count(const char* kind) {
    return callProtectedQuota(m_L, kind, protectedCount, 1);
}

int SandboxQuotaService::maxLimit(const char* kind) {
    return SandboxQuota::maxLimit(m_L, kind);
}

} // namespace Caesura
