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

// Engine lifecycle guards (audit): double-init, shutdown-without-init,
// and double-shutdown must be safe no-ops, and service access before
// init must throw (requireInitialized).
#include "entry/Engine.h"
#include "entry/EngineConfig.h"

static EngineConfig makeConfig() {
    EngineConfig cfg;
    cfg.headless = true;
    return cfg;
}

TEST_CASE("Engine: double init is rejected") {
    Engine engine(makeConfig());
    CHECK(engine.init());
    CHECK_FALSE(engine.init());  // second call refused
    engine.shutdown();
}

TEST_CASE("Engine: shutdown without init is safe + double shutdown idempotent") {
    Engine engine(makeConfig());
    engine.shutdown();          // never initialized
    engine.shutdown();          // again -- must not throw/crash
    CHECK(true);
}

TEST_CASE("Engine: service access before init throws") {
    Engine engine(makeConfig());
    bool threw = false;
    try {
        (void)engine.renderDevice();
    } catch (const std::logic_error&) {
        threw = true;
    }
    CHECK(threw);
}
