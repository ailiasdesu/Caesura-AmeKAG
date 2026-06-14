#include "doctest.h"
#include "di/TextureBudget.h"
#include <cstring>

using namespace Caesura;

TEST_CASE("TextureBudget::boundary: negative tier clamped") {
    auto& tb = TextureBudget::instance();
    int oldTier = tb.getTier();
    tb.setTier(-5);
    // Negative tier triggers auto-detect, not clamp to 0
    CHECK(tb.getTier() >= 0);
    CHECK(tb.getTier() <= 5);
    tb.setTier(oldTier);
}

TEST_CASE("TextureBudget::boundary: tier overflow clamped to 5") {
    auto& tb = TextureBudget::instance();
    int oldTier = tb.getTier();
    tb.setTier(100);
    CHECK(tb.getTier() == 5);
    tb.setTier(oldTier);
}

TEST_CASE("TextureBudget::all tiers produce valid budgets") {
    auto& tb = TextureBudget::instance();
    int oldTier = tb.getTier();
    uint32_t expected[] = {128, 256, 512, 1024, 2048, 4096};
    for (int i = 0; i <= 5; i++) {
        tb.setTier(i);
        CHECK(tb.getBudgetMB() == expected[i]);
        CHECK(tb.getBudgetBytes() == (uint64_t)expected[i] * 1024 * 1024);
    }
    tb.setTier(oldTier);
}

TEST_CASE("TextureBudget::tier names non-empty") {
    auto& tb = TextureBudget::instance();
    int oldTier = tb.getTier();
    for (int i = 0; i <= 5; i++) {
        tb.setTier(i);
        CHECK(std::strlen(tb.getTierName()) > 0);
    }
    tb.setTier(oldTier);
}
