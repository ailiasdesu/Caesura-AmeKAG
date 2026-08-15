#include "doctest.h"
#include "di/TextureBudget.h"
#include "di/SandboxQuota.h"
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
