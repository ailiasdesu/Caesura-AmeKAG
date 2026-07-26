#include "doctest.h"
#include "di/TextureBudget.h"
#include <cstring>

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
