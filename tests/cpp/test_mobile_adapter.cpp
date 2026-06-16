#include "doctest.h"
#include "platform/MobileAdapter.h"

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

// =============================================================================
// Expanded: remaining stub methods
// =============================================================================

TEST_CASE("MobileAdapter::onFingerMotion does not crash") {
    MobileAdapter ma;
    CHECK_NOTHROW(ma.onFingerMotion(150.0f, 250.0f, 0));
}

TEST_CASE("MobileAdapter::onPinch does not crash") {
    MobileAdapter ma;
    CHECK_NOTHROW(ma.onPinch(100.0f, 100.0f, 1.5f));
}

TEST_CASE("MobileAdapter::onLongPress does not crash") {
    MobileAdapter ma;
    CHECK_NOTHROW(ma.onLongPress(200.0f, 300.0f));
}

TEST_CASE("MobileAdapter::isFingerDown tracks touch state") {
    MobileAdapter ma;
    CHECK_FALSE(ma.isFingerDown(0));
    ma.onFingerDown(50.0f, 50.0f, 0);
    CHECK(ma.isFingerDown(0));
    CHECK_FALSE(ma.isFingerDown(1));  // different finger
    ma.onFingerUp(50.0f, 50.0f, 0);
    CHECK_FALSE(ma.isFingerDown(0));
}
