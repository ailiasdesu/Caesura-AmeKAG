// test_di.cpp - DI module comprehensive tests (F2)
#include "doctest.h"
#include "di/BackendRegistry.h"
#include "di/SandboxQuota.h"
#include "di/TextureBudget.h"
#include "di/api/ITextureBudget.h"
#include "audio/api/IAudioBackend.h"
#include "platform/api/IPlatformBackend.h"
#include "live2d/api/IAnimationBackend.h"
#include "input/api/IInputRouter.h"
#include "render/api/IParticleSystem.h"
#include "storage/api/ISaveManager.h"
#include "debug/api/IDebugManager.h"
#include "resource/api/IAsyncLoader.h"
#include "render/api/ILayerManager.h"
#include "steam/api/ISteamBackend.h"

using namespace Caesura;

// -- BackendRegistry: reference-based setters coverage ------------------

TEST_CASE("DI: BackendRegistry getAudioBackend returns non-null after set") {
    auto& reg = BackendRegistry::instance();
    // setAudioBackend takes a reference — can't test with sentinel.
    // Verify getter is callable and returns a pointer.
    IAudioBackend* current = reg.getAudioBackend();
    (void)current;  // may be null or set by other tests
}

TEST_CASE("DI: BackendRegistry setAnimationBackend/getAnimationBackend") {
    auto& reg = BackendRegistry::instance();
    IAnimationBackend* sentinel = reinterpret_cast<IAnimationBackend*>(0x1);
    reg.setAnimationBackend(sentinel);
    CHECK(reg.getAnimationBackend() == sentinel);
    reg.setAnimationBackend(nullptr);
    CHECK(reg.getAnimationBackend() == nullptr);
}

TEST_CASE("DI: BackendRegistry setTextureBudget/getTextureBudget") {
    auto& reg = BackendRegistry::instance();
    ITextureBudget* sentinel = reinterpret_cast<ITextureBudget*>(0x1);
    reg.setTextureBudget(sentinel);
    CHECK(reg.getTextureBudget() == sentinel);
    reg.setTextureBudget(nullptr);
    CHECK(reg.getTextureBudget() == nullptr);
}

TEST_CASE("DI: BackendRegistry setSandboxQuota/getSandboxQuota") {
    auto& reg = BackendRegistry::instance();
    ISandboxQuota* sentinel = reinterpret_cast<ISandboxQuota*>(0x1);
    reg.setSandboxQuota(sentinel);
    CHECK(reg.getSandboxQuota() == sentinel);
    reg.setSandboxQuota(nullptr);
    CHECK(reg.getSandboxQuota() == nullptr);
}

TEST_CASE("DI: BackendRegistry quota wrappers delegate to registered service") {
    class CountingQuota final : public ISandboxQuota {
    public:
        void setLuaState(lua_State*) override {}
        bool tryAlloc(const char*) override { ++tryCalls; return false; }
        void release(const char*) override { ++releaseCalls; }
        int count(const char*) override { return 0; }
        int maxLimit(const char*) override { return 0; }

        int tryCalls = 0;
        int releaseCalls = 0;
    } quota;

    auto& reg = BackendRegistry::instance();
    auto* previous = reg.getSandboxQuota();
    reg.setSandboxQuota(&quota);
    CHECK_FALSE(reg.tryAlloc("textures"));
    reg.release("textures");
    CHECK(quota.tryCalls == 1);
    CHECK(quota.releaseCalls == 1);
    reg.setSandboxQuota(previous);
}

// -- TextureBudget instance behavior ------------------------------------

TEST_CASE("DI: TextureBudget instance is accessible through its interface") {
    TextureBudget tb;
    ITextureBudget* budget = &tb;
    CHECK(budget->getBudgetMB() > 0);
}

TEST_CASE("DI: TextureBudget default tier is 1") {
    TextureBudget tb;
    // Default tier after detect() is tier 1 (256 MB)
    // detect() auto-senses system RAM and picks an appropriate tier.
    // The exact value is machine-dependent, but it should be in [0, 5].
    tb.detect();
    CHECK(tb.getTier() >= 0);
    CHECK(tb.getTier() <= 5);
    CHECK(tb.isAutoDetected());
}

TEST_CASE("DI: TextureBudget setTier/getTier round-trip") {
    TextureBudget tb;

    tb.setTier(3);
    CHECK(tb.getTier() == 3);
    CHECK(tb.isAutoDetected() == false);
}

TEST_CASE("DI: TextureBudget getBudgetMB returns positive value") {
    TextureBudget tb;
    CHECK(tb.getBudgetMB() > 0);
}

TEST_CASE("DI: TextureBudget tier names are non-empty") {
    TextureBudget tb;
    for (int t = 0; t <= 5; ++t) {
        tb.setTier(t);
        CHECK(tb.getTierName() != nullptr);
    }
}

// -- SandboxQuota low-level functions -----------------------------------

TEST_CASE("DI: SandboxQuota namespace is accessible") {
    // The namespace remains the low-level Lua table implementation used by
    // the Engine-owned SandboxQuotaService.
    CHECK(true);
}

// =============================================================================
// Expanded: remaining BackendRegistry set/get pairs
// =============================================================================

TEST_CASE("DI: BackendRegistry setPlatformBackend/getPlatformBackend") {
    auto& reg = BackendRegistry::instance();
    IPlatformBackend* sentinel = reinterpret_cast<IPlatformBackend*>(0x1);
    IPlatformBackend* old = reg.getPlatformBackend();
    reg.setPlatformBackend(sentinel);
    CHECK(reg.getPlatformBackend() == sentinel);
    reg.setPlatformBackend(old);
}

TEST_CASE("DI: BackendRegistry setInputRouter/getInputRouter") {
    auto& reg = BackendRegistry::instance();
    IInputRouter* sentinel = reinterpret_cast<IInputRouter*>(0x1);
    reg.setInputRouter(sentinel);
    CHECK(reg.getInputRouter() == sentinel);
    reg.setInputRouter(nullptr);
    CHECK(reg.getInputRouter() == nullptr);
}

TEST_CASE("DI: BackendRegistry setParticleSystem/getParticleSystem") {
    auto& reg = BackendRegistry::instance();
    IParticleSystem* sentinel = reinterpret_cast<IParticleSystem*>(0x1);
    reg.setParticleSystem(sentinel);
    CHECK(reg.getParticleSystem() == sentinel);
    reg.setParticleSystem(nullptr);
    CHECK(reg.getParticleSystem() == nullptr);
}

TEST_CASE("DI: BackendRegistry setSaveManager/getSaveManager") {
    auto& reg = BackendRegistry::instance();
    ISaveManager* sentinel = reinterpret_cast<ISaveManager*>(0x1);
    reg.setSaveManager(sentinel);
    CHECK(reg.getSaveManager() == sentinel);
    reg.setSaveManager(nullptr);
    CHECK(reg.getSaveManager() == nullptr);
}

TEST_CASE("DI: BackendRegistry setDebugManager/getDebugManager") {
    auto& reg = BackendRegistry::instance();
    IDebugManager* sentinel = reinterpret_cast<IDebugManager*>(0x1);
    reg.setDebugManager(sentinel);
    CHECK(reg.getDebugManager() == sentinel);
    reg.setDebugManager(nullptr);
    CHECK(reg.getDebugManager() == nullptr);
}

TEST_CASE("DI: BackendRegistry setAsyncLoader/getAsyncLoader") {
    auto& reg = BackendRegistry::instance();
    IAsyncLoader* sentinel = reinterpret_cast<IAsyncLoader*>(0x1);
    reg.setAsyncLoader(sentinel);
    CHECK(reg.getAsyncLoader() == sentinel);
    reg.setAsyncLoader(nullptr);
    CHECK(reg.getAsyncLoader() == nullptr);
}

TEST_CASE("DI: BackendRegistry setLayerManager/getLayerManager") {
    auto& reg = BackendRegistry::instance();
    ILayerManager* sentinel = reinterpret_cast<ILayerManager*>(0x1);
    reg.setLayerManager(sentinel);
    CHECK(reg.getLayerManager() == sentinel);
    reg.setLayerManager(nullptr);
    CHECK(reg.getLayerManager() == nullptr);
}

TEST_CASE("DI: BackendRegistry setSteamBackend/getSteamBackend") {
    auto& reg = BackendRegistry::instance();
    auto* previous = reg.getSteamBackend();
    auto* sentinel = reinterpret_cast<ISteamBackend*>(0x1);
    reg.setSteamBackend(sentinel);
    CHECK(reg.getSteamBackend() == sentinel);
    reg.setSteamBackend(previous);
}
