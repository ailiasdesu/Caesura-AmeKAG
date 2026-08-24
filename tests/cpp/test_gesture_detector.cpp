// test_gesture_detector.cpp - GestureDetector unit tests (pure state machine).
// Long-press (one finger held still) and pinch (two-finger distance ratio)
// detection against a synthetic finger stream; no SDL / GPU involvement.
#include "doctest.h"
#include "platform/GestureDetector.h"

using Caesura::GestureDetector;
using Caesura::GestureEvent;

namespace {

class EventSink {
public:
    GestureDetector det;
    int longPress = 0;
    int pinch = 0;
    float lastScale = 1.0f;
    float lastX = 0.0f, lastY = 0.0f;

    void pump(double nowMs) {
        const GestureEvent e = det.tick(nowMs);
        if (e.kind == GestureEvent::Kind::LongPress) {
            longPress++;
            lastX = e.x; lastY = e.y;
        } else if (e.kind == GestureEvent::Kind::Pinch) {
            pinch++;
            lastScale = e.scale;
            lastX = e.x; lastY = e.y;
        }
    }
};

} // namespace

TEST_CASE("GestureDetector: quick tap produces no gestures") {
    EventSink s;
    s.det.onFingerDown(0, 100, 100, 0);
    s.det.onFingerUp(0, 80);
    for (double t = 0; t < 2000; t += 16) s.pump(t);
    CHECK(s.longPress == 0);
    CHECK(s.pinch == 0);
}

TEST_CASE("GestureDetector: held still finger fires exactly one long press") {
    EventSink s;
    s.det.onFingerDown(0, 200, 300, 0);
    s.pump(100);
    s.pump(200);
    CHECK(s.longPress == 0);          // before the 500ms timeout
    s.pump(500);
    CHECK(s.longPress == 1);          // at the timeout
    CHECK(s.lastX == 200);
    CHECK(s.lastY == 300);
    s.pump(600);
    s.pump(700);
    CHECK(s.longPress == 1);          // exactly once per press
    s.det.onFingerUp(0, 800);
    s.pump(900);
    CHECK(s.longPress == 1);
}

TEST_CASE("GestureDetector: movement beyond slop cancels the long press") {
    EventSink s;
    s.det.onFingerDown(0, 100, 100, 0);
    s.det.onFingerMove(0, 140, 100, 100);   // 40px > 16px slop
    for (double t = 100; t < 1500; t += 16) s.pump(t);
    CHECK(s.longPress == 0);
    s.det.onFingerUp(0, 1600);
}

TEST_CASE("GestureDetector: two fingers apart fire pinch events") {
    EventSink s;
    // Fingers start 200px apart.
    s.det.onFingerDown(0, 500, 400, 0);
    s.det.onFingerDown(1, 700, 400, 0);
    s.pump(16);
    CHECK(s.pinch == 0);              // baseline established, no event yet

    // Spread to 320px (ratio 1.6 > 0.08 initial threshold) -> pulse.
    s.det.onFingerMove(1, 820, 400, 32);
    s.pump(48);
    CHECK(s.pinch >= 1);
    CHECK(s.lastScale == doctest::Approx(1.6));
    CHECK(s.lastX == doctest::Approx((500.0f + 820.0f) * 0.5f));

    // Small move below the 0.02 step threshold: no additional pulse.
    s.det.onFingerMove(1, 817, 400, 64);   // 317px, ratio 1.585 (delta 0.015)
    s.pump(80);
    CHECK(s.pinch == 1);

    // Crossing the 0.02 step threshold emits another pulse.
    s.det.onFingerMove(1, 760, 400, 96);   // 260px, ratio 1.30 (delta 0.285)
    s.pump(112);
    CHECK(s.pinch == 2);

    s.det.onFingerUp(0, 128);
    s.det.onFingerUp(1, 144);
    s.pump(160);
    CHECK(s.pinch == 2);
}

TEST_CASE("GestureDetector: finger release resets the gesture state") {
    EventSink s;
    s.det.onFingerDown(0, 100, 100, 0);
    s.det.onFingerDown(1, 300, 100, 0);
    s.det.onFingerUp(0, 16);
    s.det.onFingerUp(1, 32);
    // Re-hold one finger -> long press works again (previous state cleared).
    s.det.onFingerDown(2, 50, 50, 100);
    s.pump(200);
    s.pump(300);
    s.pump(400);
    s.pump(500);
    s.pump(600);
    CHECK(s.longPress == 1);
    s.det.onFingerUp(2, 700);
}

TEST_CASE("GestureDetector: activeFingerCount tracks the stream") {
    EventSink s;
    CHECK(s.det.activeFingerCount() == 0);
    s.det.onFingerDown(0, 10, 10, 0);
    s.det.onFingerDown(1, 20, 20, 0);
    CHECK(s.det.activeFingerCount() == 2);
    s.det.onFingerUp(0, 10);
    CHECK(s.det.activeFingerCount() == 1);
    s.det.reset();
    CHECK(s.det.activeFingerCount() == 0);
}
