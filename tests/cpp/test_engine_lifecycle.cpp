#include "doctest.h"
#include "../src/di/BackendRegistry.h"
#include "../src/debug/DebugManager.h"
#include "../src/job/JobSystem.h"

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

TEST_CASE("JobSystem instances are independently owned") {
    Caesura::JobSystem a;
    Caesura::JobSystem b;
    CHECK(&a != &b);
    CHECK_FALSE(a.isRunning());
    CHECK_FALSE(b.isRunning());
}
