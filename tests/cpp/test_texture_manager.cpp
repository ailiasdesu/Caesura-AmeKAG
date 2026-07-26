// test_texture_manager.cpp - TextureManager + VideoPlayer tests
#include "doctest.h"
#include "di/BackendRegistry.h"
#include "di/api/ISandboxQuota.h"
#include "di/api/ITextureBudget.h"
#include "render/TextureManager.h"
#include "render/VideoPlayer.h"
#include <cstdint>
#include <string>
#include <vector>

using namespace Caesura;

namespace {

class CountingTextureQuota final : public ISandboxQuota {
public:
    void setLuaState(lua_State*) override {}

    bool tryAlloc(const char* kind) override {
        ++tryCalls;
        kinds.emplace_back(kind ? kind : "");
        return allowAlloc;
    }

    void release(const char* kind) override {
        ++releaseCalls;
        releasedKinds.emplace_back(kind ? kind : "");
    }

    int count(const char*) override { return 0; }
    int maxLimit(const char*) override { return 0; }

    bool allowAlloc = false;
    int tryCalls = 0;
    int releaseCalls = 0;
    std::vector<std::string> kinds;
    std::vector<std::string> releasedKinds;
};

class ScopedSandboxQuotaRegistration {
public:
    explicit ScopedSandboxQuotaRegistration(ISandboxQuota* quota)
        : m_previous(BackendRegistry::instance().getSandboxQuota()) {
        BackendRegistry::instance().setSandboxQuota(quota);
    }

    ~ScopedSandboxQuotaRegistration() {
        BackendRegistry::instance().setSandboxQuota(m_previous);
    }

private:
    ISandboxQuota* m_previous;
};

class FixedTextureBudget final : public ITextureBudget {
public:
    void detect() override {}
    void setTier(int) override {}
    int getTier() const override { return 0; }
    uint32_t getBudgetMB() const override {
        return static_cast<uint32_t>(budgetBytes / (1024 * 1024));
    }
    uint64_t getBudgetBytes() const override { return budgetBytes; }
    const char* getTierName() const override { return "test"; }
    bool isAutoDetected() const override { return false; }

    uint64_t budgetBytes = 0;
};

class ScopedTextureBudgetRegistration {
public:
    explicit ScopedTextureBudgetRegistration(ITextureBudget* budget)
        : m_previous(BackendRegistry::instance().getTextureBudget()) {
        BackendRegistry::instance().setTextureBudget(budget);
    }

    ~ScopedTextureBudgetRegistration() {
        BackendRegistry::instance().setTextureBudget(m_previous);
    }

private:
    ITextureBudget* m_previous;
};

} // namespace

// =============================================================================
// TextureManager — non-GPU methods (safe per AGENTS.md section 8)
// =============================================================================

TEST_CASE("TextureManager instances are independently constructible through the interface") {
    TextureManager first;
    TextureManager second;
    ITextureManager* interface = &first;

    CHECK(interface == &first);
    CHECK(&first != &second);

    first.trackTexture(1, 4);
    CHECK(first.totalTextureBytes() == 4);
    CHECK(second.totalTextureBytes() == 0);
}



TEST_CASE("TextureManager::isValid rejects invalid IDs") {
    TextureManager tm;
    CHECK_FALSE(tm.isValid(0));
    CHECK_FALSE(tm.isValid(99999));
    CHECK_FALSE(tm.isValid(UINT32_MAX));
}

TEST_CASE("TextureManager::getTextureHandle for nonexistent ID") {
    TextureManager tm;
    CHECK(tm.getTextureHandle(0) == 0);
    CHECK(tm.getTextureHandle(99999) == 0);
}

TEST_CASE("TextureManager::getTextureSizeById for nonexistent ID") {
    TextureManager tm;
    uint16_t w = 1, h = 1;
    tm.getTextureSizeById(0, w, h);
    CHECK(w == 0);
    CHECK(h == 0);
}

TEST_CASE("TextureManager accounts maximum interface dimensions without 32-bit overflow") {
    ScopedTextureBudgetRegistration budgetRegistration(nullptr);
    TextureManager tm;

    constexpr uint64_t expectedBytes =
        uint64_t{65535} * uint64_t{65535} * uint64_t{4};
    CHECK(tm.checkBudget(7, 65535, 65535));
    CHECK(tm.totalTextureBytes() == expectedBytes);

    tm.untrackTexture(7);
    CHECK(tm.totalTextureBytes() == 0);
}

TEST_CASE("TextureManager reports budget rejection and preserves prior accounting") {
    FixedTextureBudget budget;
    budget.budgetBytes = 8;
    ScopedTextureBudgetRegistration budgetRegistration(&budget);
    TextureManager tm;

    REQUIRE(tm.checkBudget(9, 1, 1));
    REQUIRE(tm.totalTextureBytes() == 4);

    budget.budgetBytes = 3;
    CHECK_FALSE(tm.checkBudget(9, 1, 1));
    CHECK(tm.totalTextureBytes() == 4);
}

TEST_CASE("TextureManager LRU eviction uses 64-bit budget arithmetic") {
    FixedTextureBudget budget;
    budget.budgetBytes = 16;
    ScopedTextureBudgetRegistration budgetRegistration(&budget);
    TextureManager tm;

    REQUIRE(tm.checkBudget(1, 1, 1));
    REQUIRE(tm.checkBudget(2, 1, 1));
    REQUIRE(tm.totalTextureBytes() == 8);

    CHECK(tm.checkBudget(3, 2, 2));
    CHECK(tm.totalTextureBytes() == 16);
}

TEST_CASE("TextureManager device loss preserves logical budget accounting") {
    ScopedTextureBudgetRegistration budgetRegistration(nullptr);
    TextureManager tm;
    REQUIRE(tm.initialize(false));
    tm.trackTexture(17, 64);

    tm.onDeviceLost();
    CHECK(tm.totalTextureBytes() == 64);
    tm.onDeviceLost();
    CHECK(tm.totalTextureBytes() == 64);

    tm.shutdown();
    CHECK(tm.totalTextureBytes() == 0);
}

TEST_CASE("TextureManager rejects all creation paths before GPU work when quota is exhausted") {
    CountingTextureQuota quota;
    ScopedSandboxQuotaRegistration quotaRegistration(&quota);
    TextureManager tm;
    REQUIRE(tm.initialize(false));

    const uint8_t pixel[] = {255, 255, 255, 255};
    const uint8_t encoded[] = {1};
    CHECK(tm.loadTexture("missing.png") == 0);
    CHECK(tm.loadTextureFromRGBA(pixel, 1, 1) == 0);
    CHECK(tm.loadTextureFromMemory(encoded, sizeof(encoded)) == 0);
    CHECK(tm.createSolidTexture(255, 255, 255) == 0);

    CHECK(quota.tryCalls == 4);
    CHECK(quota.releaseCalls == 0);
    CHECK(quota.kinds == std::vector<std::string>(4, "textures"));

    tm.destroyTexture(99999);
    CHECK(quota.releaseCalls == 0);
    tm.shutdown();
    CHECK(quota.releaseCalls == 0);
}

TEST_CASE("TextureManager releases reservations when GPU is unavailable") {
    CountingTextureQuota quota;
    quota.allowAlloc = true;
    ScopedSandboxQuotaRegistration quotaRegistration(&quota);
    TextureManager tm;
    REQUIRE(tm.initialize(false));

    const uint8_t pixel[] = {255, 255, 255, 255};
    const uint8_t encoded[] = {1};
    CHECK(tm.loadTexture("missing.png") == 0);
    CHECK(tm.loadTextureFromRGBA(pixel, 1, 1) == 0);
    CHECK(tm.loadTextureFromMemory(encoded, sizeof(encoded)) == 0);
    CHECK(tm.createSolidTexture(255, 255, 255) == 0);

    CHECK(quota.tryCalls == 4);
    CHECK(quota.releaseCalls == 4);
    CHECK(quota.releasedKinds == std::vector<std::string>(4, "textures"));

    tm.shutdown();
    CHECK(quota.releaseCalls == 4);
}



// =============================================================================
// VideoPlayer
// =============================================================================

TEST_CASE("VideoPlayer::construct no-crash") {
    VideoPlayer vp;
    (void)vp;
}

TEST_CASE("VideoPlayer::default state") {
    VideoPlayer vp;
    CHECK_FALSE(vp.isPlaying(VideoHandle{}));
    // hasEnded with invalid handle is implementation-defined
}
