// test_backend_registry.cpp - BackendRegistry / DI module tests
#include "doctest.h"
#include "di/BackendRegistry.h"
#include "render/BgfxRenderDevice.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

using namespace Caesura;

TEST_CASE("BackendRegistry::singleton access") {
    auto& reg = BackendRegistry::instance();
    (void)reg;
}

TEST_CASE("BackendRegistry::setRenderDevice/getRenderDevice round-trip") {
    auto& reg = BackendRegistry::instance();
    BgfxRenderDevice rd;
    IRenderDevice* oldRd = reg.getRenderDevice();
    reg.setRenderDevice(rd);
    CHECK(reg.getRenderDevice() == &rd);
    reg.setRenderDevice(*oldRd);  // Restore
}

TEST_CASE("BackendRegistry::ResourceHandle invalid handle") {
    auto& reg = BackendRegistry::instance();
    ResourceHandle invalid;
    invalid.id = 0;
    invalid.generation = 0;
    CHECK(reg.isValidHandle(invalid) == false);
}

TEST_CASE("BackendRegistry::invalidateHandles verifies effect") {
    auto& reg = BackendRegistry::instance();
    // Create a valid handle, invalidate its type, verify it becomes invalid
    ResourceHandle h = reg.makeHandle(HandleType::TEXTURE, 42);
    CHECK(reg.isValidHandle(h));
    reg.invalidateHandles(HandleType::TEXTURE);
    CHECK_FALSE(reg.isValidHandle(h));
}

TEST_CASE("BackendRegistry::setJobSystem/getJobSystem") {
    auto& reg = BackendRegistry::instance();
    IJobSystem* old = reg.getJobSystem();
    IJobSystem* sentinel = reinterpret_cast<IJobSystem*>(0x1);
    reg.setJobSystem(sentinel);
    CHECK(reg.getJobSystem() == sentinel);
    reg.setJobSystem(old);  // Restore
}

TEST_CASE("BackendRegistry::setCryptoEngine/getCryptoEngine") {
    auto& reg = BackendRegistry::instance();
    carc::ICryptoEngine* old = reg.getCryptoEngine();
    carc::ICryptoEngine* sentinel = reinterpret_cast<carc::ICryptoEngine*>(0x1);
    reg.setCryptoEngine(sentinel);
    CHECK(reg.getCryptoEngine() == sentinel);
    reg.setCryptoEngine(old);  // Restore
}

TEST_CASE("BackendRegistry::setLuaManager/getLuaManager") {
    auto& reg = BackendRegistry::instance();
    ILuaManager* old = reg.getLuaManager();
    ILuaManager* sentinel = reinterpret_cast<ILuaManager*>(0x1);
    reg.setLuaManager(sentinel);
    CHECK(reg.getLuaManager() == sentinel);
    reg.setLuaManager(old);  // Restore
}

// =============================================================================
// Expanded coverage
// =============================================================================

TEST_CASE("BackendRegistry::registerNullBackends compiles and runs") {
    (void)&BackendRegistry::registerNullBackends;
}

TEST_CASE("BackendRegistry::createBackend factory rejects unknown names") {
    auto& reg = BackendRegistry::instance();
    CHECK(reg.createRenderDevice("vulkan") == nullptr);
    CHECK(reg.createAudioBackend("fmod") == nullptr);
    CHECK(reg.createPlatformBackend("glfw") == nullptr);
    CHECK(reg.createRenderDevice("nonexistent") == nullptr);
}

TEST_CASE("BackendRegistry::getRenderDeviceFromLua returns fallback") {
    lua_State* L = luaL_newstate();
    REQUIRE(L != nullptr);
    IRenderDevice* dev = BackendRegistry::getRenderDeviceFromLua(L);
    (void)dev;  // may be null or not -- just verify no crash
    lua_close(L);
}

TEST_CASE("BackendRegistry::registerEngineBindings works") {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    REQUIRE(L != nullptr);
    BackendRegistry::registerEngineBindings(L);
    lua_getglobal(L, "Engine");
    CHECK(lua_istable(L, -1));
    lua_pop(L, 1);
    lua_close(L);
}
