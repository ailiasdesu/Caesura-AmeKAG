// test_gesture_detector.cpp - GestureDetector unit tests (pure state machine).
// Long-press, pinch, two-finger tap, three-finger hold, and swipe down/up
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
    int twoFingerTap = 0;
    int threeFingerHold = 0;
    int swipeDown = 0;
    int swipeUp = 0;
    float lastScale = 1.0f;
    float lastX = 0.0f, lastY = 0.0f;
    float lastDeltaX = 0.0f, lastDeltaY = 0.0f;

    void pump(double nowMs) {
        const GestureEvent e = det.tick(nowMs);
        if (e.kind == GestureEvent::Kind::LongPress) {
            longPress++;
            lastX = e.x; lastY = e.y;
        } else if (e.kind == GestureEvent::Kind::Pinch) {
            pinch++;
            lastScale = e.scale;
            lastX = e.x; lastY = e.y;
        } else if (e.kind == GestureEvent::Kind::TwoFingerTap) {
            twoFingerTap++;
            lastX = e.x; lastY = e.y;
        } else if (e.kind == GestureEvent::Kind::ThreeFingerHold) {
            threeFingerHold++;
            lastX = e.x; lastY = e.y;
        } else if (e.kind == GestureEvent::Kind::SwipeDown) {
            swipeDown++;
            lastX = e.x; lastY = e.y;
            lastDeltaX = e.deltaX; lastDeltaY = e.deltaY;
        } else if (e.kind == GestureEvent::Kind::SwipeUp) {
            swipeUp++;
            lastX = e.x; lastY = e.y;
            lastDeltaX = e.deltaX; lastDeltaY = e.deltaY;
        }
    }
};

} // namespace

TEST_CASE("GestureDetector: quick tap produces no long press or pinch") {
    EventSink s;
    s.det.onFingerDown(0, 100, 100, 0);
    s.det.onFingerUp(0, 80);
    for (double t = 0; t < 2000; t += 16) s.pump(t);
    CHECK(s.longPress == 0);
    CHECK(s.pinch == 0);
    CHECK(s.swipeDown == 0);
    CHECK(s.swipeUp == 0);
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

// =============================================================================
// M5: Mobile Touch Gestures (TwoFingerTap, ThreeFingerHold, SwipeDown, SwipeUp)
// =============================================================================

TEST_CASE("GestureDetector: TwoFingerTap detected when released within 300ms") {
    EventSink s;
    s.det.onFingerDown(0, 100.0f, 200.0f, 0.0);
    s.det.onFingerDown(1, 200.0f, 200.0f, 10.0);
    s.det.onFingerUp(0, 120.0);
    s.det.onFingerUp(1, 130.0);
    s.pump(140.0);

    CHECK(s.twoFingerTap == 1);
    CHECK(s.lastX == doctest::Approx(150.0f));
    CHECK(s.lastY == doctest::Approx(200.0f));
}

TEST_CASE("GestureDetector: TwoFingerTap rejected if fingers move beyond slop") {
    EventSink s;
    s.det.onFingerDown(0, 100.0f, 200.0f, 0.0);
    s.det.onFingerDown(1, 200.0f, 200.0f, 10.0);
    s.det.onFingerMove(0, 140.0f, 200.0f, 50.0); // 40px > 16px slop
    s.det.onFingerUp(0, 120.0);
    s.det.onFingerUp(1, 130.0);
    s.pump(140.0);

    CHECK(s.twoFingerTap == 0);
}

TEST_CASE("GestureDetector: TwoFingerTap rejected if held longer than 300ms") {
    EventSink s;
    s.det.onFingerDown(0, 100.0f, 200.0f, 0.0);
    s.det.onFingerDown(1, 200.0f, 200.0f, 10.0);
    s.det.onFingerUp(0, 400.0); // 400ms > 300ms threshold
    s.det.onFingerUp(1, 410.0);
    s.pump(420.0);

    CHECK(s.twoFingerTap == 0);
}

TEST_CASE("GestureDetector: ThreeFingerHold detected when 3 fingers held for >= 200ms") {
    EventSink s;
    s.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
    s.det.onFingerDown(1, 200.0f, 100.0f, 0.0);
    s.det.onFingerDown(2, 300.0f, 100.0f, 0.0);

    s.pump(100.0);
    CHECK(s.threeFingerHold == 0);

    s.pump(200.0);
    CHECK(s.threeFingerHold == 1);
    CHECK(s.lastX == doctest::Approx(200.0f)); // centroid (100+200+300)/3
    CHECK(s.lastY == doctest::Approx(100.0f));

    // Subsequent ticks do not re-trigger repeatedly while held
    s.pump(300.0);
    CHECK(s.threeFingerHold == 1);

    s.det.onFingerUp(0, 350.0);
    s.det.onFingerUp(1, 360.0);
    s.det.onFingerUp(2, 370.0);
}

TEST_CASE("GestureDetector: ThreeFingerHold cancelled if any finger moves beyond slop") {
    EventSink s;
    s.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
    s.det.onFingerDown(1, 200.0f, 100.0f, 0.0);
    s.det.onFingerDown(2, 300.0f, 100.0f, 0.0);

    s.det.onFingerMove(2, 350.0f, 100.0f, 50.0); // 50px > 16px slop
    s.pump(200.0);
    CHECK(s.threeFingerHold == 0);
}

TEST_CASE("GestureDetector: SwipeDown detected on single finger vertical downward drag") {
    EventSink s;
    s.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
    s.det.onFingerMove(0, 105.0f, 170.0f, 50.0);
    s.det.onFingerUp(0, 100.0);
    s.pump(110.0);

    CHECK(s.swipeDown == 1);
    CHECK(s.swipeUp == 0);
    CHECK(s.lastDeltaY == doctest::Approx(70.0f));
    CHECK(s.lastX == doctest::Approx(105.0f));
    CHECK(s.lastY == doctest::Approx(170.0f));
}

TEST_CASE("GestureDetector: SwipeUp detected on single finger vertical upward drag") {
    EventSink s;
    s.det.onFingerDown(0, 100.0f, 200.0f, 0.0);
    s.det.onFingerMove(0, 95.0f, 120.0f, 50.0);
    s.det.onFingerUp(0, 100.0);
    s.pump(110.0);

    CHECK(s.swipeUp == 1);
    CHECK(s.swipeDown == 0);
    CHECK(s.lastDeltaY == doctest::Approx(-80.0f));
    CHECK(s.lastX == doctest::Approx(95.0f));
    CHECK(s.lastY == doctest::Approx(120.0f));
}

TEST_CASE("GestureDetector: Horizontal movement is not recognized as vertical swipe") {
    EventSink s;
    s.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
    s.det.onFingerMove(0, 200.0f, 110.0f, 50.0); // dx = 100, dy = 10
    s.det.onFingerUp(0, 100.0);
    s.pump(110.0);

    CHECK(s.swipeDown == 0);
    CHECK(s.swipeUp == 0);
}

// =============================================================================
// C6 收口: threshold BOUNDARY tests — "must NOT fire" is asserted as carefully
// as "must fire". Every constant is read from GestureDetector, so retuning a
// threshold moves these tests with it instead of silently invalidating them,
// and each pair brackets one threshold from both sides.
//
// The native thresholds are aligned with web/touch-gestures.js (C7) — see the
// comparison table in GestureDetector.h. These cases therefore also pin the
// two-端 agreement: retuning one side only makes the paired case here fail.
// =============================================================================

TEST_CASE("GestureDetector boundary: swipe just under 50 px does not fire, exactly 50 px does") {
    {
        EventSink s;
        const float justUnder = GestureDetector::kSwipeMinDistance - 1.0f; // 49
        s.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
        s.det.onFingerMove(0, 100.0f, 100.0f + justUnder, 50.0);
        s.det.onFingerUp(0, 100.0);
        for (double t = 100.0; t < 220.0; t += 16.0) s.pump(t);
        CHECK(s.swipeDown == 0);
        CHECK(s.swipeUp == 0);
    }
    {
        EventSink s;
        s.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
        s.det.onFingerMove(0, 100.0f, 100.0f + GestureDetector::kSwipeMinDistance, 50.0);
        s.det.onFingerUp(0, 100.0);
        s.pump(110.0);
        CHECK(s.swipeDown == 1);
        CHECK(s.lastDeltaY == doctest::Approx(GestureDetector::kSwipeMinDistance));
    }
}

TEST_CASE("GestureDetector boundary: dominance ratio 1.5 brackets the diagonal case") {
    {
        // dy = 60 clears the distance gate, but dx = 50 so |dy| < 1.5*|dx| = 75.
        EventSink s;
        s.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
        s.det.onFingerMove(0, 150.0f, 160.0f, 50.0);
        s.det.onFingerUp(0, 100.0);
        for (double t = 100.0; t < 220.0; t += 16.0) s.pump(t);
        CHECK(s.swipeDown == 0);
        CHECK(s.swipeUp == 0);
    }
    {
        // dx = 40, dy = 60 == 1.5 * 40 -> inclusive comparison accepts it.
        EventSink s;
        s.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
        s.det.onFingerMove(0, 140.0f, 160.0f, 50.0);
        s.det.onFingerUp(0, 100.0);
        s.pump(110.0);
        CHECK(s.swipeDown == 1);
    }
}

TEST_CASE("GestureDetector boundary: two-finger tap at exactly 300 ms fires, 1 ms later does not") {
    {
        EventSink s;
        s.det.onFingerDown(0, 100.0f, 200.0f, 0.0);
        s.det.onFingerDown(1, 200.0f, 200.0f, 0.0);
        s.det.onFingerUp(0, GestureDetector::kTwoFingerTapMaxMs);
        s.det.onFingerUp(1, GestureDetector::kTwoFingerTapMaxMs);
        s.pump(GestureDetector::kTwoFingerTapMaxMs + 10.0);
        CHECK(s.twoFingerTap == 1);
    }
    {
        EventSink s;
        s.det.onFingerDown(0, 100.0f, 200.0f, 0.0);
        s.det.onFingerDown(1, 200.0f, 200.0f, 0.0);
        s.det.onFingerUp(0, GestureDetector::kTwoFingerTapMaxMs + 1.0);
        s.det.onFingerUp(1, GestureDetector::kTwoFingerTapMaxMs + 1.0);
        for (double t = 310.0; t < 440.0; t += 16.0) s.pump(t);
        CHECK(s.twoFingerTap == 0);
    }
}

TEST_CASE("GestureDetector boundary: two-finger tap tolerance is 20 px, not the 16 px hold slop") {
    // Web parity: twoFingerTapMaxMovePx == 20. An 18 px wobble exceeds the
    // long-press slop (16) yet must STILL read as a tap; 22 px must not.
    {
        EventSink s;
        s.det.onFingerDown(0, 100.0f, 200.0f, 0.0);
        s.det.onFingerDown(1, 200.0f, 200.0f, 0.0);
        s.det.onFingerMove(0, 118.0f, 200.0f, 50.0);
        s.det.onFingerUp(0, 100.0);
        s.det.onFingerUp(1, 110.0);
        s.pump(120.0);
        CHECK(s.twoFingerTap == 1);
    }
    {
        EventSink s;
        s.det.onFingerDown(0, 100.0f, 200.0f, 0.0);
        s.det.onFingerDown(1, 200.0f, 200.0f, 0.0);
        s.det.onFingerMove(0, 122.0f, 200.0f, 50.0);
        s.det.onFingerUp(0, 100.0);
        s.det.onFingerUp(1, 110.0);
        for (double t = 120.0; t < 250.0; t += 16.0) s.pump(t);
        CHECK(s.twoFingerTap == 0);
    }
}

TEST_CASE("GestureDetector boundary: a wobble that returns to its origin is still not a tap") {
    // maxTravel semantics: comparing only the FINAL offset would accept this,
    // even though the finger travelled 40 px away and back.
    EventSink s;
    s.det.onFingerDown(0, 100.0f, 200.0f, 0.0);
    s.det.onFingerDown(1, 200.0f, 200.0f, 0.0);
    s.det.onFingerMove(0, 140.0f, 200.0f, 30.0);
    s.det.onFingerMove(0, 100.0f, 200.0f, 60.0);
    s.det.onFingerUp(0, 100.0);
    s.det.onFingerUp(1, 110.0);
    for (double t = 120.0; t < 250.0; t += 16.0) s.pump(t);
    CHECK(s.twoFingerTap == 0);
}

TEST_CASE("GestureDetector boundary: three-finger hold brackets 200 ms") {
    EventSink s;
    s.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
    s.det.onFingerDown(1, 200.0f, 100.0f, 0.0);
    s.det.onFingerDown(2, 300.0f, 100.0f, 0.0);
    s.pump(GestureDetector::kThreeFingerHoldMs - 1.0);
    CHECK(s.threeFingerHold == 0);
    s.pump(GestureDetector::kThreeFingerHoldMs);
    CHECK(s.threeFingerHold == 1);
}

TEST_CASE("GestureDetector boundary: three-finger hold tolerates 22 px but not 30 px of wobble") {
    // Web parity: threeFingerHoldMaxMovePx == 25, deliberately looser than the
    // 16 px single-finger slop (three resting fingers wobble more than one).
    {
        EventSink s;
        s.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
        s.det.onFingerDown(1, 200.0f, 100.0f, 0.0);
        s.det.onFingerDown(2, 300.0f, 100.0f, 0.0);
        s.det.onFingerMove(2, 322.0f, 100.0f, 50.0);
        s.pump(250.0);
        CHECK(s.threeFingerHold == 1);
    }
    {
        EventSink s;
        s.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
        s.det.onFingerDown(1, 200.0f, 100.0f, 0.0);
        s.det.onFingerDown(2, 300.0f, 100.0f, 0.0);
        s.det.onFingerMove(2, 330.0f, 100.0f, 50.0);
        for (double t = 250.0; t < 520.0; t += 16.0) s.pump(t);
        CHECK(s.threeFingerHold == 0);
    }
}

TEST_CASE("GestureDetector boundary: long press keeps its own 16 px slop") {
    // The looser multi-finger tolerances must not have relaxed the shipped
    // single-finger long-press behavior.
    {
        EventSink s;
        s.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
        s.det.onFingerMove(0, 114.0f, 100.0f, 50.0); // 14 px < 16 -> still held
        s.pump(GestureDetector::kLongPressMs + 10.0);
        CHECK(s.longPress == 1);
    }
    {
        EventSink s;
        s.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
        s.det.onFingerMove(0, 118.0f, 100.0f, 50.0); // 18 px > 16 -> a drag
        for (double t = 510.0; t < 900.0; t += 16.0) s.pump(t);
        CHECK(s.longPress == 0);
    }
}

TEST_CASE("GestureDetector boundary: lifting the last of two fingers is not a swipe") {
    // Sequence guard (m_seqMaxFingers): a two-finger drag ends with one finger
    // still down, and that final lift must not decay into a one-finger swipe.
    EventSink s;
    s.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
    s.det.onFingerDown(1, 200.0f, 100.0f, 0.0);
    s.det.onFingerMove(0, 100.0f, 300.0f, 50.0);   // 200 px, far past 50
    s.det.onFingerMove(1, 200.0f, 300.0f, 50.0);
    s.det.onFingerUp(1, 400.0);
    s.det.onFingerUp(0, 420.0);
    for (double t = 430.0; t < 580.0; t += 16.0) s.pump(t);
    CHECK(s.swipeDown == 0);
    CHECK(s.swipeUp == 0);
    CHECK(s.twoFingerTap == 0);                    // moved too far, held too long
}

TEST_CASE("GestureDetector boundary: a single-finger swipe still fires after a multi-finger sequence") {
    // The sequence guard must clear once every finger is up, otherwise the
    // first swipe after any two-finger gesture would be swallowed forever.
    EventSink s;
    s.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
    s.det.onFingerDown(1, 200.0f, 100.0f, 0.0);
    s.det.onFingerUp(1, 50.0);
    s.det.onFingerUp(0, 60.0);
    for (double t = 70.0; t < 220.0; t += 16.0) s.pump(t);
    const int tapsFromFirstSequence = s.twoFingerTap;

    s.det.onFingerDown(0, 100.0f, 100.0f, 1000.0);
    s.det.onFingerMove(0, 100.0f, 200.0f, 1050.0);
    s.det.onFingerUp(0, 1100.0);
    s.pump(1110.0);
    CHECK(s.swipeDown == 1);
    CHECK(s.twoFingerTap == tapsFromFirstSequence);
}

// ---------------------------------------------------------------------------
// Interaction between gestures that share a starting shape. Two fingers can
// become a pinch, a tap, or (with a third) a hold, and tick() returns at most
// one event per call -- so the question is not "does each gesture work" but
// "can two of them fire off one physical gesture".
// ---------------------------------------------------------------------------

TEST_CASE("GestureDetector overlap: a pinch too small to start is still only one verdict") {
    EventSink s;
    s.det.onFingerDown(0, 300.0f, 300.0f, 0.0);
    s.det.onFingerDown(1, 400.0f, 300.0f, 10.0);   // 100 px apart
    s.pump(20.0);
    s.det.onFingerMove(0, 302.0f, 300.0f, 40.0);   // squeeze 4 px: ratio 0.96,
    s.det.onFingerMove(1, 398.0f, 300.0f, 40.0);   // under the 0.08 threshold
    s.pump(60.0);
    CHECK(s.pinch == 0);
    s.det.onFingerUp(0, 120.0);
    s.det.onFingerUp(1, 130.0);
    for (double t = 140.0; t < 400.0; t += 16.0) s.pump(t);
    // 2 px of travel released inside 300 ms IS a tap by the documented
    // contract; what must not happen is both a pinch and a tap.
    CHECK(s.twoFingerTap == 1);
    CHECK(s.pinch == 0);
}

TEST_CASE("GestureDetector overlap: a real pinch release is not also a tap") {
    EventSink s;
    s.det.onFingerDown(0, 300.0f, 300.0f, 0.0);
    s.det.onFingerDown(1, 400.0f, 300.0f, 10.0);
    s.pump(20.0);
    s.det.onFingerMove(0, 250.0f, 300.0f, 40.0);   // 50 px outward: past both
    s.det.onFingerMove(1, 450.0f, 300.0f, 40.0);   // the tap slop and 0.08
    s.pump(60.0);
    CHECK(s.pinch >= 1);
    s.det.onFingerUp(0, 100.0);
    s.det.onFingerUp(1, 110.0);
    for (double t = 120.0; t < 500.0; t += 16.0) s.pump(t);
    CHECK(s.twoFingerTap == 0);
}

TEST_CASE("GestureDetector overlap: three fingers held long do not also long-press") {
    // Long press requires exactly one finger, so three held past 500 ms must
    // yield the hold and nothing else -- including after the extra time that
    // would have satisfied the long-press timer.
    EventSink s;
    s.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
    s.det.onFingerDown(1, 200.0f, 100.0f, 5.0);
    s.det.onFingerDown(2, 300.0f, 100.0f, 10.0);
    for (double t = 20.0; t < 900.0; t += 16.0) s.pump(t);
    CHECK(s.threeFingerHold == 1);
    CHECK(s.longPress == 0);
}

TEST_CASE("GestureDetector overlap: 2 to 3 to 2 fingers is no tap, and the guard clears") {
    // The sequence peaked at three fingers, so dropping back to two and
    // releasing is not a two-finger tap. The guard must also clear afterwards:
    // one that stays armed swallows every later gesture, the same failure shape
    // the swipe guard had.
    EventSink s;
    s.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
    s.det.onFingerDown(1, 200.0f, 100.0f, 10.0);
    s.det.onFingerDown(2, 300.0f, 100.0f, 20.0);
    s.pump(40.0);
    s.det.onFingerUp(2, 60.0);
    s.pump(80.0);
    s.det.onFingerUp(0, 100.0);
    s.det.onFingerUp(1, 110.0);
    for (double t = 120.0; t < 400.0; t += 16.0) s.pump(t);
    CHECK(s.twoFingerTap == 0);

    s.det.onFingerDown(0, 500.0f, 500.0f, 500.0);
    s.det.onFingerDown(1, 560.0f, 500.0f, 505.0);
    s.pump(520.0);
    s.det.onFingerUp(0, 600.0);
    s.det.onFingerUp(1, 610.0);
    for (double t = 620.0; t < 900.0; t += 16.0) s.pump(t);
    CHECK(s.twoFingerTap == 1);
}

TEST_CASE("GestureDetector overlap: two consecutive two-finger taps both fire") {
    EventSink s;
    for (int round = 0; round < 2; ++round) {
        const double base = round * 1000.0;
        s.det.onFingerDown(0, 100.0f, 100.0f, base);
        s.det.onFingerDown(1, 160.0f, 100.0f, base + 5.0);
        s.pump(base + 20.0);
        s.det.onFingerUp(0, base + 100.0);
        s.det.onFingerUp(1, base + 110.0);
        for (double t = base + 120.0; t < base + 400.0; t += 16.0) s.pump(t);
    }
    CHECK(s.twoFingerTap == 2);
}

TEST_CASE("GestureDetector overlap: lift order does not change the verdict") {
    EventSink a;
    a.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
    a.det.onFingerDown(1, 160.0f, 100.0f, 5.0);
    a.pump(20.0);
    a.det.onFingerUp(0, 100.0);
    a.det.onFingerUp(1, 110.0);
    for (double t = 120.0; t < 400.0; t += 16.0) a.pump(t);

    EventSink b;
    b.det.onFingerDown(0, 100.0f, 100.0f, 0.0);
    b.det.onFingerDown(1, 160.0f, 100.0f, 5.0);
    b.pump(20.0);
    b.det.onFingerUp(1, 100.0);   // reversed
    b.det.onFingerUp(0, 110.0);
    for (double t = 120.0; t < 400.0; t += 16.0) b.pump(t);

    CHECK(a.twoFingerTap == b.twoFingerTap);
    CHECK(a.twoFingerTap == 1);
    CHECK(a.swipeDown == b.swipeDown);
    CHECK(a.swipeUp == b.swipeUp);
}

