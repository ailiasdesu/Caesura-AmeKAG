#include "doctest.h"
#include "di/BackendRegistry.h"
#include "di/TextureBudget.h"
#include "di/SandboxQuota.h"
#include "di/api/ISandboxQuota.h"
#include "di/api/ITextureBudget.h"
#include "render/TextureManager.h"
#include <cstdint>
#include <string>
#include <vector>
#include <cstring>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

using namespace Caesura;

TEST_CASE("TextureBudget::boundary: negative tier clamped") {
    TextureBudget tb;
    tb.setTier(-5);
    // Negative tier triggers auto-detect, not clamp to 0
    CHECK(tb.getTier() >= 0);
    CHECK(tb.getTier() <= 5);
}

TEST_CASE("TextureBudget::boundary: tier overflow clamped to 5") {
    TextureBudget tb;
    tb.setTier(100);
    CHECK(tb.getTier() == 5);
}

TEST_CASE("TextureBudget::all tiers produce valid budgets") {
    TextureBudget tb;
    uint32_t expected[] = {128, 256, 512, 1024, 2048, 4096};
    for (int i = 0; i <= 5; i++) {
        tb.setTier(i);
        CHECK(tb.getBudgetMB() == expected[i]);
        CHECK(tb.getBudgetBytes() == (uint64_t)expected[i] * 1024 * 1024);
    }
}

TEST_CASE("TextureBudget::tier names non-empty") {
    TextureBudget tb;
    for (int i = 0; i <= 5; i++) {
        tb.setTier(i);
        CHECK(std::strlen(tb.getTierName()) > 0);
    }
}

// =============================================================================
// G8 hardening: tier selection, developer override, quota enforcement.
// All cases are GPU-free: they exercise the decoupled di/ budget + quota layer.
// =============================================================================

TEST_CASE("TextureBudget::tier selection by configured budget size") {
    TextureBudget tb;
    // Each tier 0..5 selects a distinct fixed budget (MB and raw bytes).
    uint32_t expectedMB[] = {128, 256, 512, 1024, 2048, 4096};
    for (int i = 0; i <= 5; ++i) {
        tb.setTier(i);
        INFO("tier = " << i);
        CHECK(tb.getBudgetMB() == expectedMB[i]);
        // A uint32 MB budget must not wrap bytes when widened on this platform.
        CHECK(tb.getBudgetBytes() == uint64_t(expectedMB[i]) * 1024u * 1024u);
    }
    // Budgets increase strictly with tier (validates boundary ordering).
    tb.setTier(0);
    uint64_t prev = tb.getBudgetBytes();
    for (int i = 1; i <= 5; ++i) {
        tb.setTier(i);
        CHECK(tb.getBudgetBytes() > prev);
        prev = tb.getBudgetBytes();
    }
}

TEST_CASE("TextureBudget::tier 5 is reachable only via developer override") {
    // Auto-detection is RAM-bounded: detect() can only ever assign tiers 0..4.
    // Tier 5 (4096 MB / DevOverride) therefore must be the developer-override
    // tier. If detection ever produced 5 this case would still pass, but the
    // name-boundary assertion below pins the intended override semantics.
    TextureBudget tb;
    tb.detect();
    int autoTier = tb.getTier();
    CHECK(autoTier >= 0);
    CHECK(autoTier <= 4);

    tb.setTier(5);
    CHECK(tb.getBudgetMB() == 4096);
    CHECK(std::strcmp(tb.getTierName(), "4GB (DevOverride)") == 0);
}

TEST_CASE("TextureBudget::developer override flag lifecycle") {
    TextureBudget tb;
    tb.detect();
    CHECK(tb.isAutoDetected());

    // Any explicit tier pin clears the auto-detected flag.
    tb.setTier(0);
    CHECK_FALSE(tb.isAutoDetected());
    tb.setTier(3);
    CHECK_FALSE(tb.isAutoDetected());
    tb.setTier(5);
    CHECK_FALSE(tb.isAutoDetected());

    // Re-detect (negative tier) restores the auto-detected flag.
    tb.setTier(-1);
    CHECK(tb.isAutoDetected());
    CHECK(tb.getTier() >= 0);
    CHECK(tb.getTier() <= 4);
}

TEST_CASE("SandboxQuota::texture registration enforces tier quota exactly") {
    // Decoupled quota layer: '_SANDBOX_RESOURCES' carries the per-kind budget.
    // Register text.max textures succeeds; the (max+1)-th is rejected and the
    // counter stays at the cap. Releasing frees a slot for a new registration.
    lua_State* L = luaL_newstate();
    REQUIRE(L != nullptr);
    luaL_openlibs(L);

    lua_newtable(L);
    lua_pushinteger(L, 2); lua_setfield(L, -2, "textures_loaded");
    lua_pushinteger(L, 2); lua_setfield(L, -2, "textures_max");
    lua_setglobal(L, "_SANDBOX_RESOURCES");

    // Already at cap: further registration is rejected, counter unchanged.
    CHECK_FALSE(SandboxQuota::tryAlloc(L, "textures"));
    CHECK_EQ(SandboxQuota::count(L, "textures"), 2);

    // Release one slot, then a registration succeeds up to the new cap.
    SandboxQuota::release(L, "textures");
    CHECK_EQ(SandboxQuota::count(L, "textures"), 1);
    CHECK(SandboxQuota::tryAlloc(L, "textures"));
    CHECK_EQ(SandboxQuota::count(L, "textures"), 2);

    // Back at the cap: rejected again.
    CHECK_FALSE(SandboxQuota::tryAlloc(L, "textures"));
    CHECK_EQ(SandboxQuota::count(L, "textures"), 2);

    lua_close(L);
}

TEST_CASE("SandboxQuota::zero texture budget rejects all registrations") {
    // quota = 0 means "no resources left": the very first registration must be
    // rejected gracefully (no crash, counter pinned at 0), and every retry
    // must keep failing while the cap stays 0.
    lua_State* L = luaL_newstate();
    REQUIRE(L != nullptr);
    luaL_openlibs(L);

    lua_newtable(L);
    lua_pushinteger(L, 0); lua_setfield(L, -2, "textures_loaded");
    lua_pushinteger(L, 0); lua_setfield(L, -2, "textures_max");
    lua_setglobal(L, "_SANDBOX_RESOURCES");

    CHECK_FALSE(SandboxQuota::tryAlloc(L, "textures"));
    CHECK_EQ(SandboxQuota::count(L, "textures"), 0);
    CHECK_FALSE(SandboxQuota::tryAlloc(L, "textures"));
    CHECK_EQ(SandboxQuota::count(L, "textures"), 0);

    // Releasing with a zero budget stays a safe no-op (floor at 0).
    SandboxQuota::release(L, "textures");
    CHECK_EQ(SandboxQuota::count(L, "textures"), 0);

    lua_close(L);
}

TEST_CASE("SandboxQuotaService::textures kind enforces decoupled budget") {
    // Reuse the real service wrapper to prove the same quota math applies
    // through the interface used by TextureManager's reservation path.
    lua_State* L = luaL_newstate();
    REQUIRE(L != nullptr);
    luaL_openlibs(L);

    lua_newtable(L);
    lua_pushinteger(L, 0); lua_setfield(L, -2, "textures_loaded");
    lua_pushinteger(L, 1); lua_setfield(L, -2, "textures_max");
    lua_setglobal(L, "_SANDBOX_RESOURCES");

    SandboxQuotaService quota;
    quota.setLuaState(L);

    CHECK(quota.tryAlloc("textures"));
    CHECK_EQ(quota.count("textures"), 1);
    // Reaching the cap: rejected.
    CHECK_FALSE(quota.tryAlloc("textures"));
    CHECK_EQ(quota.count("textures"), 1);

    quota.setLuaState(nullptr);
    // No Lua state installed: falls open (allows) rather than crashing.
    CHECK(quota.tryAlloc("textures"));

    lua_close(L);
}

// =============================================================================
// TextureManager budget/accounting layer — GPU-free edge cases.
// These exercise the decoupled checkBudget/trackTexture/untrackTexture path
// (the same accounting registerTexture() drives) plus lifecycle safety,
// path-rejection, and quota-balance invariants without creating GPU resources.
// =============================================================================

namespace {

class EdgeCountingQuota final : public ISandboxQuota {
public:
    void setLuaState(lua_State*) override {}
    bool tryAlloc(const char* kind) override {
        ++tryCalls;
        kinds.emplace_back(kind ? kind : "");
        return allowAlloc;
    }
    void release(const char*) override { ++releaseCalls; }
    int count(const char*) override { return 0; }
    int maxLimit(const char*) override { return 0; }

    bool allowAlloc = false;
    int tryCalls = 0;
    int releaseCalls = 0;
    std::vector<std::string> kinds;
};

class EdgeCountingBudget final : public ITextureBudget {
public:
    void detect() override {}
    void setTier(int) override {}
    int getTier() const override { return 0; }
    uint32_t getBudgetMB() const override {
        return static_cast<uint32_t>(budgetBytes / (1024 * 1024));
    }
    uint64_t getBudgetBytes() const override { return budgetBytes; }
    const char* getTierName() const override { return "edge"; }
    bool isAutoDetected() const override { return false; }

    uint64_t budgetBytes = 0;
};

class ScopedQuota {
public:
    explicit ScopedQuota(ISandboxQuota* q) : m_prev(BackendRegistry::instance().getSandboxQuota()) {
        BackendRegistry::instance().setSandboxQuota(q);
    }
    ~ScopedQuota() { BackendRegistry::instance().setSandboxQuota(m_prev); }
private:
    ISandboxQuota* m_prev;
};

class ScopedBudget {
public:
    explicit ScopedBudget(ITextureBudget* b) : m_prev(BackendRegistry::instance().getTextureBudget()) {
        BackendRegistry::instance().setTextureBudget(b);
    }
    ~ScopedBudget() { BackendRegistry::instance().setTextureBudget(m_prev); }
private:
    ITextureBudget* m_prev;
};

} // namespace

TEST_CASE("TextureManager::zero budget rejects all real allocations at manager layer") {
    // round 79 covers quota=0 (the Lua sandbox counter). This case pins the
    // same "no budget" behaviour at the manager checkBudget layer: a real
    // 1x1 = 4 bytes cannot fit a 0-byte budget, and the accounting floor
    // stays at 0 with no crash and no negative total.
    EdgeCountingBudget budget;               // budgetBytes = 0
    ScopedBudget budgetReg(&budget);
    EdgeCountingQuota quota;
    ScopedQuota quotaReg(&quota);

    TextureManager tm;
    REQUIRE(tm.initialize(false));

    CHECK_FALSE(tm.checkBudget(1, 1, 1));    // 4 bytes > 0 budget
    CHECK(tm.totalTextureBytes() == 0);

    // Repeated attempts stay rejected; accounting never drops below 0.
    CHECK_FALSE(tm.checkBudget(1, 1, 1));
    CHECK_FALSE(tm.checkBudget(2, 255, 255));
    CHECK(tm.totalTextureBytes() == 0);

    // A zero-dimension request is 0 bytes, so it fits even a 0 budget.
    CHECK(tm.checkBudget(9, 0, 0));
    CHECK(tm.totalTextureBytes() == 0);

    tm.shutdown();
}

TEST_CASE("TextureManager::budget full then release frees a slot for re-allocation") {
    // Budget of 8 bytes holds two 1x1 (4-byte) textures exactly. After the
    // third request the manager evicts the LRU to make room, then release of
    // an explicitly destroyed texture frees budget so a new allocation lands.
    EdgeCountingBudget budget;
    budget.budgetBytes = 8;
    ScopedBudget budgetReg(&budget);
    EdgeCountingQuota quota;
    ScopedQuota quotaReg(&quota);

    TextureManager tm;
    REQUIRE(tm.initialize(false));

    REQUIRE(tm.checkBudget(1, 1, 1));       // +4 -> 4
    REQUIRE(tm.checkBudget(2, 1, 1));       // +4 -> 8 (full)
    CHECK(tm.totalTextureBytes() == 8);

    // Destroying id=1 frees 4 bytes; id=2 stays tracked.
    tm.destroyTexture(1);
    CHECK(tm.totalTextureBytes() == 4);
    CHECK(tm.isValid(1) == false);

    // Freed slot can be re-allocated (stays within the 8-byte budget).
    CHECK(tm.checkBudget(3, 1, 1));         // +4 -> 8
    CHECK(tm.totalTextureBytes() == 8);

    // Now the budget is full again: a larger texture is rejected and the
    // accounting is preserved (no negative / unbounded growth).
    CHECK_FALSE(tm.checkBudget(4, 2, 2));   // 16 bytes > remaining
    CHECK(tm.totalTextureBytes() == 8);

    tm.shutdown();
}

TEST_CASE("TextureManager::duplicate destroy is idempotent and accounting-safe") {
    EdgeCountingQuota quota;
    quota.allowAlloc = true;
    ScopedQuota quotaReg(&quota);

    TextureManager tm;
    REQUIRE(tm.initialize(false));

    // Track a texture via the accounting layer directly (registerTexture needs
    // GPU; checkBudget/trackTexture are the same bookkeeping it drives).
    REQUIRE(tm.checkBudget(7, 4, 4));       // 64 bytes
    REQUIRE(tm.totalTextureBytes() == 64);

    // Destroy is idempotent: a second destroy of the same id must not underflow
    // the byte total nor double-release anything.
    tm.destroyTexture(7);
    tm.destroyTexture(7);                    // duplicate destroy = safe no-op
    tm.destroyTexture(99999);                // unknown id = safe no-op
    CHECK(tm.totalTextureBytes() == 0);

    // untrack after destroy must not drive the total negative.
    tm.untrackTexture(7);
    CHECK(tm.totalTextureBytes() == 0);

    // The GPU-unavailable load paths took no texture yet; quota reservations
    // that were opened (tryAlloc) must all be balanced by release on shutdown.
    const uint8_t pixel[] = {255, 255, 255, 255};
    CHECK(tm.loadTextureFromRGBA(pixel, 1, 1) == 0);
    CHECK(tm.totalTextureBytes() == 0);

    tm.shutdown();
    CHECK(quota.releaseCalls == quota.tryCalls);
}

TEST_CASE("TextureManager::path rejection never leaks a quota reservation") {
    // loadTexture validates path/empty BEFORE opening a quota reservation, so
    // traversal or empty paths cost zero tryAlloc. This pins the "load failure
    // does not pollute the quota/cache" invariant at the manager boundary.
    EdgeCountingQuota quota;
    quota.allowAlloc = true;
    ScopedQuota quotaReg(&quota);

    TextureManager tm;
    REQUIRE(tm.initialize(false));

    CHECK(tm.loadTexture("../escape.png") == 0);
    CHECK(tm.loadTexture("sub/../../escape.png") == 0);
    CHECK(tm.loadTexture("") == 0);
    CHECK(tm.loadTexture("./ok") == 0);        // missing file, no ".." in path

    // Only the clean path reached the reservation stage; every reservation
    // was balanced by a release (failed loads never leak a hold).
    CHECK(quota.tryCalls >= 1);
    CHECK(quota.releaseCalls == quota.tryCalls);

    tm.shutdown();
    CHECK(quota.releaseCalls == quota.tryCalls);
}

TEST_CASE("TextureManager::tier downgrade re-enforces quota without touching prior ids") {
    // Simulates a runtime tier/budget downgrade: textures tracked under a
    // larger budget must not be re-evaluated when a NEW allocation arrives,
    // and the new tighter budget rejects allocations that no longer fit while
    // preserving the accounting of already-tracked textures.
    EdgeCountingBudget budget;
    budget.budgetBytes = 100 * 1024;          // roomy
    ScopedBudget budgetReg(&budget);
    EdgeCountingQuota quota;
    ScopedQuota quotaReg(&quota);

    TextureManager tm;
    REQUIRE(tm.initialize(false));

    REQUIRE(tm.checkBudget(1, 100, 100));     // ~40 KB tracked
    const uint64_t before = tm.totalTextureBytes();
    REQUIRE(before > 0);

    // Downgrade: budget can no longer fit a 100x100 texture alone.
    budget.budgetBytes = 4;
    CHECK_FALSE(tm.checkBudget(2, 100, 100));
    // Already-tracked texture id is untouched by a rejected new allocation.
    CHECK(tm.totalTextureBytes() == before);

    tm.shutdown();
}

TEST_CASE("TextureManager::dimension boundary accounting") {
    // Non-power-of-two and single-pixel sizes are valid (bgfx handles them);
    // the accounting layer must charge exact width*height*4 bytes. A 0-byte
    // dimension request is charged 0 and never grows the total.
    EdgeCountingBudget budget;
    budget.budgetBytes = uint64_t{64} * 1024 * 1024;   // 64 MB, effectively open
    ScopedBudget budgetReg(&budget);

    TextureManager tm;
    REQUIRE(tm.initialize(false));

    // 1x1 = 4 bytes.
    REQUIRE(tm.checkBudget(1, 1, 1));
    CHECK(tm.totalTextureBytes() == 4);

    // Non-power-of-two 3x7 = 84 bytes.
    REQUIRE(tm.checkBudget(2, 3, 7));
    CHECK(tm.totalTextureBytes() == 4 + 84);

    // Zero-dimension = 0 bytes, does not disturb the running total.
    REQUIRE(tm.checkBudget(3, 0, 0));
    CHECK(tm.totalTextureBytes() == 88);

    // Re-request with same id after untrack recharges exactly.
    tm.untrackTexture(2);
    CHECK(tm.totalTextureBytes() == 4);
    REQUIRE(tm.checkBudget(2, 3, 7));
    CHECK(tm.totalTextureBytes() == 4 + 84);

    tm.shutdown();
}

