#include "doctest.h"
#include "../src/di/BackendRegistry.h"
#include "../src/debug/DebugManager.h"
#include "../src/job/JobSystem.h"
#include "../src/entry/Engine.h"
#include "../src/entry/EngineConfig.h"
#include "../src/render/NullRenderDevice.h"
#include "../src/audio/NullAudioBackend.h"
#include "../src/platform/NullPlatformBackend.h"
#include "../src/minigame/NullMiniGameBackend.h"
#include <memory>

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
    CHECK_NOTHROW(engine.shutdown());  // again -- must not throw/crash
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

// ============================================================================
// Engine lifecycle boundary tests (task): clean round-trip, defaults,
// explicit backend overrides, and destroy-after-shutdown safety.
// ============================================================================

// (a) construct + init + shutdown completes cleanly with no crash / no
// double-free. shutdown() is idempotent after a successful init.
TEST_CASE("Engine: clean construct+init+shutdown roundtrip") {
    Engine engine(makeConfig());
    REQUIRE(engine.init());
    engine.shutdown();
    CHECK_NOTHROW(engine.shutdown());  // second shutdown must be a no-op
}

// (f) destroy after shutdown -- the documented lifecycle order -- must be
// safe. The destructor observes m_shutdownComplete and must NOT re-enter
// shutdown (no double-free of the owned backends).
TEST_CASE("Engine: destroy after shutdown is safe") {
    auto engine = std::make_unique<Engine>(makeConfig());
    REQUIRE(engine->init());
    engine->shutdown();
    engine.reset();  // destructor runs with m_shutdownComplete already set
    CHECK(true);     // reaching here means destroy-after-shutdown is safe
}

// (f-alt) destroying an initialized but never-shutdown Engine is also safe:
// the destructor must auto-shutdown (idempotent), freeing all owned backends.
TEST_CASE("Engine: destroy after init (no explicit shutdown) is safe") {
    auto engine = std::make_unique<Engine>(makeConfig());
    REQUIRE(engine->init());
    engine.reset();  // destructor triggers shutdown()
    CHECK(true);
}

// (d-i) EngineConfig carries sane defaults and leaves all backend pointers
// null, so the Engine's composition root fills in placeholder backends.
TEST_CASE("EngineConfig: defaults yield a working minimal config") {
    EngineConfig cfg;
    CHECK(cfg.title == std::string("Caesura (AmeKAG)"));
    CHECK(cfg.width == 1280);
    CHECK(cfg.height == 720);
    CHECK_FALSE(cfg.headless);
    CHECK_FALSE(cfg.editorMode);
    CHECK(cfg.frameLimit == 0);
    CHECK(cfg.render == nullptr);
    CHECK(cfg.audio == nullptr);
    CHECK(cfg.platform == nullptr);
    CHECK(cfg.gpuMonitor == nullptr);
    CHECK(cfg.miniGame == nullptr);
    CHECK(cfg.animation == nullptr);

    // With no explicit backends the engine still builds a minimal, working
    // config: init succeeds and the registry is populated with placeholder
    // (Null) backends.
    Engine engine(makeConfig());
    REQUIRE(engine.init());
    auto& reg = BackendRegistry::instance();
    CHECK(reg.getRenderDevice() != nullptr);
    CHECK(reg.getAudioBackend() != nullptr);
    CHECK(reg.getPlatformBackend() != nullptr);
    CHECK(reg.getMiniGameBackend() != nullptr);
    CHECK(reg.getAnimationBackend() != nullptr);
    CHECK(reg.getJobSystem() != nullptr);
    engine.shutdown();
    // After shutdown the registry must be unregistered again.
    CHECK(reg.getRenderDevice() == nullptr);
    CHECK(reg.getAudioBackend() == nullptr);
}

// (e) Explicit backend overrides injected through EngineConfig are wired
// through to the registry (verified by pointer equality). Engine takes
// ownership of the injected objects, so the test must NOT free them.
TEST_CASE("Engine: explicit backend overrides wire through to registry") {
    auto* injectedRender   = new NullRenderDevice();
    auto* injectedAudio    = new NullAudioBackend();
    auto* injectedPlatform = new NullPlatformBackend();
    auto* injectedMiniGame = new NullMiniGameBackend();

    EngineConfig cfg;
    cfg.headless   = true;
    cfg.render     = injectedRender;
    cfg.audio      = injectedAudio;
    cfg.platform   = injectedPlatform;
    cfg.miniGame   = injectedMiniGame;

    {
        Engine engine(std::move(cfg));  // ownership transfers to Engine
        REQUIRE(engine.init());
        BackendRegistry& reg = BackendRegistry::instance();
        CHECK(reg.getRenderDevice() == injectedRender);
        CHECK(reg.getAudioBackend() == injectedAudio);
        CHECK(reg.getPlatformBackend() == injectedPlatform);
        CHECK(reg.getMiniGameBackend() == injectedMiniGame);
        engine.shutdown();
    }  // ~Engine frees the injected backends it now owns (no double-free)
    CHECK(BackendRegistry::instance().getRenderDevice() == nullptr);
}
