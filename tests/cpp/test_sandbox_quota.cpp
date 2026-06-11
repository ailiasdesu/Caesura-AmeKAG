#include "doctest.h"
#include "../src/di/SandboxQuota.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

using namespace Caesura;

TEST_CASE("SandboxQuota::tryAlloc within limit") {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    lua_newtable(L);
    lua_setglobal(L, "_SANDBOX_RESOURCES");
    bool ok = SandboxQuota::tryAlloc(L, "textures");
    CHECK(ok);
    int cnt = SandboxQuota::count(L, "textures");
    CHECK_EQ(cnt, 1);
    lua_close(L);
}

TEST_CASE("SandboxQuota::release decrements") {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    lua_newtable(L);
    lua_setglobal(L, "_SANDBOX_RESOURCES");
    SandboxQuota::tryAlloc(L, "audio_handles");
    SandboxQuota::tryAlloc(L, "audio_handles");
    CHECK_EQ(SandboxQuota::count(L, "audio_handles"), 2);
    SandboxQuota::release(L, "audio_handles");
    CHECK_EQ(SandboxQuota::count(L, "audio_handles"), 1);
    lua_close(L);
}

TEST_CASE("SandboxQuota::release clamps at zero") {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    lua_newtable(L);
    lua_setglobal(L, "_SANDBOX_RESOURCES");
    SandboxQuota::release(L, "particles_emitters");
    CHECK_EQ(SandboxQuota::count(L, "particles_emitters"), 0);
    lua_close(L);
}

TEST_CASE("SandboxQuota::maxLimit reads from Lua table") {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    // Set up _SANDBOX_RESOURCES with max values (mimics sandbox.lua init)
    lua_newtable(L);
    lua_pushinteger(L, 256); lua_setfield(L, -2, "textures_max");
    lua_pushinteger(L, 64);  lua_setfield(L, -2, "audio_handles_max");
    lua_pushinteger(L, 128); lua_setfield(L, -2, "rtt_canvases_max");
    lua_pushinteger(L, 32);  lua_setfield(L, -2, "particles_emitters_max");
    lua_setglobal(L, "_SANDBOX_RESOURCES");
    CHECK_EQ(SandboxQuota::maxLimit(L, "textures"), 256);
    CHECK_EQ(SandboxQuota::maxLimit(L, "audio_handles"), 64);
    CHECK_EQ(SandboxQuota::maxLimit(L, "rtt_canvases"), 128);
    CHECK_EQ(SandboxQuota::maxLimit(L, "particles_emitters"), 32);
    lua_close(L);
}

TEST_CASE("SandboxQuota::maxLimit defaults to 0 for empty table") {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    lua_newtable(L);
    lua_setglobal(L, "_SANDBOX_RESOURCES");
    CHECK_EQ(SandboxQuota::maxLimit(L, "textures"), 0);
    lua_close(L);
}

TEST_CASE("SandboxQuota::unknown kind returns 0") {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    lua_newtable(L);
    lua_setglobal(L, "_SANDBOX_RESOURCES");
    CHECK_EQ(SandboxQuota::count(L, "nonexistent"), 0);
    CHECK_EQ(SandboxQuota::maxLimit(L, "nonexistent"), 0);
    lua_close(L);
}
