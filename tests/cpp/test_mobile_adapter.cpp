#include "doctest.h"
#include "platform/MobileAdapter.h"
#include <SDL3/SDL.h>
#include <cmath>

using namespace Caesura;

namespace {

// Synchronous event probe: SDL_PushEvent() invokes watchers registered with
// SDL_AddEventWatch() *before* queueing, so injection can be verified in
// unit tests without SDL_Init / a window.
struct EventProbe {
    SDL_Event last{};
    int count = 0;
    static bool SDLCALL watch(void* userdata, SDL_Event* ev) {
        auto* probe = static_cast<EventProbe*>(userdata);
        probe->last = *ev;
        probe->count++;
        return true;
    }
};

// RAII guard: the watch must be removed even when a REQUIRE fails and the
// test unwinds, otherwise SDL's global watcher list keeps a dangling pointer.
struct EventProbeGuard {
    EventProbe probe;
    EventProbeGuard() { SDL_AddEventWatch(EventProbe::watch, &probe); }
    ~EventProbeGuard() { SDL_RemoveEventWatch(EventProbe::watch, &probe); }
};

} // namespace

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

TEST_CASE("MobileAdapter::lifecycle resume with null state and saved data") {
    MobileAdapter ma;
    // No Lua state available in unit tests: nullptr must be safe and
    // the paused flag must still toggle.
    ma.onPause(nullptr);
    CHECK(ma.isPaused());
    ma.onResume(nullptr, "slot_3");
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

// =============================================================================
// Touch state machine (fixed in P4-3): counting must not drift
// =============================================================================

TEST_CASE("MobileAdapter::multi-touch counting") {
    MobileAdapter ma;
    ma.onFingerDown(10.0f, 10.0f, 0);
    ma.onFingerDown(20.0f, 20.0f, 1);
    ma.onFingerDown(30.0f, 30.0f, 2);
    CHECK(ma.activeTouchCount() == 3);
    CHECK(ma.isFingerDown(0));
    CHECK(ma.isFingerDown(1));
    CHECK(ma.isFingerDown(2));

    ma.onFingerUp(20.0f, 20.0f, 1);
    CHECK(ma.activeTouchCount() == 2);
    CHECK_FALSE(ma.isFingerDown(1));
    CHECK(ma.isFingerDown(0));
    CHECK(ma.isFingerDown(2));

    ma.onFingerUp(10.0f, 10.0f, 0);
    ma.onFingerUp(30.0f, 30.0f, 2);
    CHECK(ma.activeTouchCount() == 0);
}

TEST_CASE("MobileAdapter::duplicate finger down does not double-count") {
    MobileAdapter ma;
    ma.onFingerDown(10.0f, 10.0f, 0);
    ma.onFingerDown(15.0f, 15.0f, 0);  // duplicate down, same finger
    CHECK(ma.activeTouchCount() == 1);
    CHECK(ma.isFingerDown(0));
    ma.onFingerUp(15.0f, 15.0f, 0);
    CHECK(ma.activeTouchCount() == 0);
}

TEST_CASE("MobileAdapter::out-of-range fingers are ignored") {
    MobileAdapter ma;
    ma.onFingerDown(10.0f, 10.0f, 99);   // out of range
    ma.onFingerDown(10.0f, 10.0f, -1);   // negative id
    CHECK(ma.activeTouchCount() == 0);
    CHECK_FALSE(ma.isFingerDown(99));

    ma.onFingerUp(10.0f, 10.0f, 99);     // up without down, out of range
    CHECK(ma.activeTouchCount() == 0);   // must not go negative

    ma.onFingerDown(10.0f, 10.0f, 0);
    CHECK(ma.activeTouchCount() == 1);
    ma.onFingerUp(10.0f, 10.0f, 99);     // up of unknown finger
    CHECK(ma.activeTouchCount() == 1);   // real finger untouched
}

TEST_CASE("MobileAdapter::finger up without down does not underflow") {
    MobileAdapter ma;
    ma.onFingerUp(10.0f, 10.0f, 0);
    ma.onFingerUp(10.0f, 10.0f, 0);
    CHECK(ma.activeTouchCount() == 0);
}

// =============================================================================
// Event injection (verified synchronously via SDL_AddEventWatch)
// =============================================================================

TEST_CASE("MobileAdapter::touch injects scaled mouse events") {
    EventProbeGuard g;

    MobileAdapter ma;
    ma.setDisplayScale(2.0f);

    ma.onFingerDown(10.0f, 20.0f, 0);
    REQUIRE(g.probe.count == 1);
    CHECK(g.probe.last.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
    CHECK(g.probe.last.button.x == doctest::Approx(20.0f));
    CHECK(g.probe.last.button.y == doctest::Approx(40.0f));
    CHECK(g.probe.last.button.button == SDL_BUTTON_LEFT);

    ma.onFingerMotion(15.0f, 25.0f, 0);
    REQUIRE(g.probe.count == 2);
    CHECK(g.probe.last.type == SDL_EVENT_MOUSE_MOTION);
    CHECK(g.probe.last.motion.x == doctest::Approx(30.0f));
    CHECK(g.probe.last.motion.y == doctest::Approx(50.0f));

    ma.onFingerUp(15.0f, 25.0f, 0);
    REQUIRE(g.probe.count == 3);
    CHECK(g.probe.last.type == SDL_EVENT_MOUSE_BUTTON_UP);
    CHECK(g.probe.last.button.button == SDL_BUTTON_LEFT);
}

TEST_CASE("MobileAdapter::duplicate down and untracked motion inject nothing") {
    EventProbeGuard g;

    MobileAdapter ma;
    ma.onFingerDown(10.0f, 10.0f, 0);
    CHECK(g.probe.count == 1);

    ma.onFingerDown(11.0f, 11.0f, 0);   // duplicate down: no new event
    CHECK(g.probe.count == 1);

    ma.onFingerMotion(12.0f, 12.0f, 1); // motion of untracked finger: none
    CHECK(g.probe.count == 1);

    ma.onFingerMotion(12.0f, 12.0f, 0); // motion of tracked finger
    CHECK(g.probe.count == 2);
}

TEST_CASE("MobileAdapter::long press injects right-click pair") {
    EventProbeGuard g;

    MobileAdapter ma;
    ma.onLongPress(100.0f, 200.0f);
    REQUIRE(g.probe.count == 2);
    CHECK(g.probe.last.type == SDL_EVENT_MOUSE_BUTTON_UP);
    CHECK(g.probe.last.button.button == SDL_BUTTON_RIGHT);
}

// =============================================================================
// Pinch → wheel mapping
// =============================================================================

TEST_CASE("MobileAdapter::pinch establishes baseline then maps delta to wheel") {
    EventProbeGuard g;

    MobileAdapter ma;
    ma.onPinch(100.0f, 100.0f, 1.0f);          // baseline, no event
    CHECK(g.probe.count == 0);
    CHECK(ma.getLastPinchScale() == doctest::Approx(1.0f));

    ma.onPinch(100.0f, 100.0f, 1.25f);         // +0.25 → wheel y=+25
    REQUIRE(g.probe.count == 1);
    CHECK(g.probe.last.type == SDL_EVENT_MOUSE_WHEEL);
    CHECK(g.probe.last.wheel.y == doctest::Approx(25.0f));
    CHECK(ma.getLastPinchScale() == doctest::Approx(1.25f));

    ma.onPinch(100.0f, 100.0f, 1.0f);          // zoom back out: -0.25
    REQUIRE(g.probe.count == 2);
    CHECK(g.probe.last.wheel.y == doctest::Approx(-25.0f));

    ma.onPinch(100.0f, 100.0f, 1.0f);          // no delta: no event
    CHECK(g.probe.count == 2);
}

TEST_CASE("MobileAdapter::resetPinch ends gesture") {
    EventProbeGuard g;

    MobileAdapter ma;
    ma.onPinch(100.0f, 100.0f, 1.0f);
    CHECK(ma.getLastPinchScale() == doctest::Approx(1.0f));

    ma.resetPinch();
    CHECK(ma.getLastPinchScale() == 0.0f);

    ma.onPinch(100.0f, 100.0f, 2.0f);          // new gesture: baseline only
    CHECK(g.probe.count == 0);
    CHECK(ma.getLastPinchScale() == doctest::Approx(2.0f));
}

TEST_CASE("MobileAdapter::non-finite inputs are rejected") {
    EventProbeGuard g;

    MobileAdapter ma;
    ma.onFingerDown(NAN, 10.0f, 0);            // NaN x: ignored
    ma.onFingerDown(10.0f, INFINITY, 1);       // Inf y: ignored
    CHECK(ma.activeTouchCount() == 0);
    CHECK(g.probe.count == 0);

    ma.onPinch(100.0f, 100.0f, NAN);           // NaN scale: no baseline poison
    CHECK(ma.getLastPinchScale() == 0.0f);
    ma.onPinch(100.0f, 100.0f, 1.5f);          // still a fresh baseline
    CHECK(g.probe.count == 0);
    CHECK(ma.getLastPinchScale() == doctest::Approx(1.5f));

    ma.onFingerDown(10.0f, 10.0f, 0);
    CHECK(ma.activeTouchCount() == 1);
    ma.onFingerMotion(NAN, 10.0f, 0);          // NaN motion: ignored
    CHECK(g.probe.count == 1);                 // only the valid down event
    ma.onFingerUp(10.0f, NAN, 0);              // NaN up: state unchanged
    CHECK(ma.activeTouchCount() == 1);
    CHECK(ma.isFingerDown(0));

    ma.onLongPress(INFINITY, 10.0f);           // Inf long press: ignored
    CHECK(g.probe.count == 1);

    ma.setDisplayScale(NAN);                   // NaN scale rejected -> 1.0
    CHECK(ma.getDisplayScale() == doctest::Approx(1.0f));
}
