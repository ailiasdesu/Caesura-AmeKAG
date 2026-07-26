#include "doctest.h"
#include "di/BackendRegistry.h"
#include "di/TextureBudget.h"
#include "debug/DebugManager.h"
#include "resource/ResourceHandle.h"
#include <cstring>

using namespace Caesura;

TEST_CASE("BackendRegistry::singleton") {
    auto& a = BackendRegistry::instance();
    auto& b = BackendRegistry::instance();
    CHECK(&a == &b);
}

TEST_CASE("BackendRegistry::null-backend getters") {
    auto& reg = BackendRegistry::instance();
    CHECK(reg.getRenderDevice() == nullptr);
    CHECK(reg.getAudioBackend() == nullptr);
}

TEST_CASE("BackendRegistry::ResourceHandle generation tracking") {
    auto& reg = BackendRegistry::instance();
    GenerationTracker tracker;
    auto* old = reg.getResourceGenerationTracker();
    reg.setResourceGenerationTracker(&tracker);
    auto h1 = tracker.makeHandle(HandleType::TEXTURE, 1);
    auto h2 = tracker.makeHandle(HandleType::TEXTURE, 2);
    CHECK(h1.id != 0);
    CHECK(h2.id != 0);
    CHECK(h1.id != h2.id);
    CHECK(tracker.isCurrent(h1));
    ResourceHandle zero;
    const bool zeroIsValid = zero.id != 0 && tracker.isCurrent(zero);
    CHECK_FALSE(zeroIsValid);
    tracker.invalidate(HandleType::TEXTURE);
    CHECK_FALSE(tracker.isCurrent(h1));
    CHECK_FALSE(tracker.isCurrent(h2));
    auto h3 = tracker.makeHandle(HandleType::AUDIO, 1);
    CHECK(tracker.isCurrent(h3));
    reg.setResourceGenerationTracker(old);
}

TEST_CASE("TextureBudget instances keep independent state") {
    TextureBudget a;
    TextureBudget b;
    a.setTier(0);
    b.setTier(5);
    CHECK(a.getTier() == 0);
    CHECK(b.getTier() == 5);
}

TEST_CASE("TextureBudget::detect produces valid tier") {
    TextureBudget tb;
    tb.detect();
    int tier = tb.getTier();
    CHECK(tier >= 0);
    CHECK(tier <= 4);
    CHECK(tb.isAutoDetected());
    uint32_t mb = tb.getBudgetMB();
    CHECK(mb >= 128);
    CHECK(mb <= 2048);
}

TEST_CASE("TextureBudget::manual override") {
    TextureBudget tb;
    tb.setTier(5);
    CHECK(tb.getTier() == 5);
    CHECK_FALSE(tb.isAutoDetected());
    CHECK(tb.getBudgetMB() == 4096);
    CHECK(tb.getBudgetBytes() == 4096ULL * 1024 * 1024);
    tb.setTier(-1);  // reset to auto
    CHECK(tb.isAutoDetected());
}

TEST_CASE("TextureBudget::tier names non-null") {
    TextureBudget tb;
    tb.setTier(0);
    CHECK(strlen(tb.getTierName()) > 0);
    tb.setTier(5);
    CHECK(strlen(tb.getTierName()) > 0);
}

TEST_CASE("DebugManager::singleton") {
    auto& a = DebugManager::instance();
    auto& b = DebugManager::instance();
    CHECK(&a == &b);
}

TEST_CASE("DebugManager::init rejects path traversal") {
    auto& dm = DebugManager::instance();
    CHECK_FALSE(dm.init("../etc"));
}

TEST_CASE("DebugManager::valid init and log") {
    auto& dm = DebugManager::instance();
    CHECK(dm.init("logs"));
    uint32_t before = dm.entryCount();
    dm.log(DbgLevel::Info, SubSys::Engine, ErrCode::Ok, "unit test message");
    CHECK(dm.entryCount() > before);
    dm.log(DbgLevel::Err, SubSys::Engine, ErrCode::Engine_RenderInitFailed, "test error");
    CHECK(dm.errorCount() >= 1);
    dm.shutdown();
}

// =============================================================================
// Expanded DebugManager coverage
// =============================================================================

TEST_CASE("DebugManager::shutdown is idempotent") {
    auto& dm = DebugManager::instance();
    dm.shutdown();
    dm.shutdown();  // second call must not crash
}

TEST_CASE("DebugManager::ring buffer wraps after 1024 entries") {
    auto& dm = DebugManager::instance();
    CHECK(dm.init("logs"));
    // Fill ring buffer past capacity
    for (int i = 0; i < 1100; ++i) {
        dm.log(DbgLevel::Info, SubSys::Engine, ErrCode::Ok, "msg %d", i);
    }
    // Ring buffer should be capped at kRingSize (1024)
    CHECK(dm.ringBuffer().size() <= 1024);
    CHECK(dm.ringBuffer().size() > 0);
    dm.shutdown();
}

TEST_CASE("DebugManager::lastError returns most recent error") {
    auto& dm = DebugManager::instance();
    CHECK(dm.init("logs"));
    dm.log(DbgLevel::Err, SubSys::Render, ErrCode::Render_ShaderCompileFailed, "shader A failed");
    dm.log(DbgLevel::Warn, SubSys::Audio, ErrCode::Ok, "audio warning");
    dm.log(DbgLevel::Err, SubSys::Engine, ErrCode::Engine_AudioInitFailed, "final error");
    const auto* last = dm.lastError();
    CHECK(last != nullptr);
    CHECK(last->subsystem == SubSys::Engine);
    dm.shutdown();
}

TEST_CASE("DebugManager::subsystemErrorCount isolates subsystems") {
    auto& dm = DebugManager::instance();
    CHECK(dm.init("logs"));
    // Read baseline — previous tests may have logged to any subsystem
    uint32_t baseRender = dm.subsystemErrorCount(SubSys::Render);
    uint32_t baseAudio  = dm.subsystemErrorCount(SubSys::Audio);
    uint32_t baseEngine = dm.subsystemErrorCount(SubSys::Engine);
    dm.log(DbgLevel::Err, SubSys::Render, ErrCode::Ok, "rerr");
    dm.log(DbgLevel::Err, SubSys::Render, ErrCode::Ok, "rerr2");
    dm.log(DbgLevel::Err, SubSys::Audio, ErrCode::Ok, "aerr");
    CHECK(dm.subsystemErrorCount(SubSys::Render) >= baseRender + 2);
    CHECK(dm.subsystemErrorCount(SubSys::Audio) >= baseAudio + 1);
    // Engine was not touched by this test — count unchanged
    CHECK(dm.subsystemErrorCount(SubSys::Engine) == baseEngine);
    dm.shutdown();
}

TEST_CASE("DebugManager::dumpFullReport returns non-empty string") {
    auto& dm = DebugManager::instance();
    CHECK(dm.init("logs"));
    dm.log(DbgLevel::Info, SubSys::Engine, ErrCode::Ok, "report test");
    std::string report = dm.dumpFullReport();
    CHECK_FALSE(report.empty());
    dm.shutdown();
}

TEST_CASE("DebugManager::setRenderInfo/getRenderInfo round-trip") {
    auto& dm = DebugManager::instance();
    DebugManager::RenderInfo ri;
    ri.backendName = "D3D11";
    ri.width = 1920; ri.height = 1080; ri.viewCount = 4; ri.shaderReady = true;
    dm.setRenderInfo(ri);
    auto out = dm.getRenderInfo();
    CHECK(out.width == 1920);
    CHECK(out.height == 1080);
    CHECK(out.shaderReady);
    CHECK(out.viewCount == 4);
}

TEST_CASE("DebugManager::setAudioInfo/getAudioInfo round-trip") {
    auto& dm = DebugManager::instance();
    DebugManager::AudioInfo ai;
    ai.initialized = true; ai.bgmBusReady = true; ai.voiceBusReady = false;
    ai.seBusReady = true; ai.globalVolume = 0.75f;
    dm.setAudioInfo(ai);
    auto out = dm.getAudioInfo();
    CHECK(out.initialized);
    CHECK(out.bgmBusReady);
    CHECK_FALSE(out.voiceBusReady);
    CHECK(out.seBusReady);
    CHECK(out.globalVolume == 0.75f);
}
