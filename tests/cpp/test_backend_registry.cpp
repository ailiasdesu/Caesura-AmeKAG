// test_backend_registry.cpp - BackendRegistry / DI module tests
#include "doctest.h"
#include "di/BackendRegistry.h"
#include "render/BgfxRenderDevice.h"

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

TEST_CASE("BackendRegistry::invalidateHandles") {
    auto& reg = BackendRegistry::instance();
    reg.invalidateHandles(HandleType::TEXTURE);
}

TEST_CASE("BackendRegistry::setJobSystem/getJobSystem") {
    auto& reg = BackendRegistry::instance();
    IJobSystem* before = reg.getJobSystem();
    (void)before;
}

TEST_CASE("BackendRegistry::setCryptoEngine/getCryptoEngine") {
    auto& reg = BackendRegistry::instance();
    carc::ICryptoEngine* before = reg.getCryptoEngine();
    (void)before;
}

TEST_CASE("BackendRegistry::setLuaManager/getLuaManager") {
    auto& reg = BackendRegistry::instance();
    ILuaManager* before = reg.getLuaManager();
    (void)before;
}
