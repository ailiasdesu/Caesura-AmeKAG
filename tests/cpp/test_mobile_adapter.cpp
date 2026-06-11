#include "doctest.h"
#include "Core/MobileAdapter.h"

using namespace Caesura;

TEST_CASE("MobileAdapter::stub returns safe defaults") {
    MobileAdapter ma;
    CHECK_FALSE(ma.isPaused());
    CHECK(ma.activeTouchCount() == 0);
}

TEST_CASE("MobileAdapter::display scale") {
    MobileAdapter ma;
    CHECK(ma.getDisplayScale() == doctest::Approx(1.0f));
    ma.setDisplayScale(2.0f);
    CHECK(ma.getDisplayScale() == doctest::Approx(2.0f));
}

TEST_CASE("MobileAdapter::lifecycle stubs do not crash") {
    MobileAdapter ma;
    ma.onPause(nullptr);
    CHECK(ma.isPaused());
    ma.onResume(nullptr);
    CHECK_FALSE(ma.isPaused());
}
