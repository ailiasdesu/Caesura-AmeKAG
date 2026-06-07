#include "doctest.h"
#include "Core/BackendRegistry.h"
#include "Core/TextureBudget.h"
#include "Core/DebugManager.h"
#include "Resource/ResourceHandle.h"
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
    auto h1 = reg.makeHandle(HandleType::TEXTURE, 1);
    auto h2 = reg.makeHandle(HandleType::TEXTURE, 2);
    CHECK(h1.id != 0);
    CHECK(h2.id != 0);
    CHECK(h1.id != h2.id);
    CHECK(reg.isValidHandle(h1));
    ResourceHandle zero;
    CHECK_FALSE(reg.isValidHandle(zero));
    reg.invalidateHandles(HandleType::TEXTURE);
    CHECK_FALSE(reg.isValidHandle(h1));
    CHECK_FALSE(reg.isValidHandle(h2));
    auto h3 = reg.makeHandle(HandleType::AUDIO, 1);
    CHECK(reg.isValidHandle(h3));
}

TEST_CASE("TextureBudget::singleton") {
    auto& a = TextureBudget::instance();
    auto& b = TextureBudget::instance();
    CHECK(&a == &b);
}

TEST_CASE("TextureBudget::detect produces valid tier") {
    auto& tb = TextureBudget::instance();
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
    auto& tb = TextureBudget::instance();
    tb.setTier(5);
    CHECK(tb.getTier() == 5);
    CHECK_FALSE(tb.isAutoDetected());
    CHECK(tb.getBudgetMB() == 4096);
    CHECK(tb.getBudgetBytes() == 4096ULL * 1024 * 1024);
    tb.setTier(-1);  // reset to auto
    CHECK(tb.isAutoDetected());
}

TEST_CASE("TextureBudget::tier names non-null") {
    auto& tb = TextureBudget::instance();
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