#include "doctest.h"
#include "../src/Core/BackendRegistry.h"
#include "../src/Debug/DebugManager.h"
#include "../src/Core/JobSystem.h"

using namespace Caesura;

// Engine lifecycle integration tests

TEST_CASE("BackendRegistry::singleton") {
    auto& a = Caesura::BackendRegistry::instance();
    auto& b = Caesura::BackendRegistry::instance();
    CHECK(&a == &b);
}

TEST_CASE("BackendRegistry::null backends at startup") {
    auto& reg = Caesura::BackendRegistry::instance();
    CHECK(reg.getRenderDevice() == nullptr);
    CHECK(reg.getAudioBackend() == nullptr);
}

TEST_CASE("BackendRegistry::miniGame null initially") {
    auto& reg = Caesura::BackendRegistry::instance();
    CHECK(reg.getMiniGameBackend() == nullptr);
}

TEST_CASE("DebugManager::singleton") {
    auto& a = Caesura::DebugManager::instance();
    auto& b = Caesura::DebugManager::instance();
    CHECK(&a == &b);
}

TEST_CASE("JobSystem::singleton") {
    auto& a = Caesura::JobSystem::instance();
    auto& b = Caesura::JobSystem::instance();
    CHECK(&a == &b);
}
