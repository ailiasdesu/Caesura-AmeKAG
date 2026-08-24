#pragma once

#include <cstdint>

namespace Caesura {

// GestureDetector — raw finger-stream (SDL touch) → discrete gesture events.
//
// Pure platform-layer state machine (no SDL dependency, no rendering): the
// Engine feeds FINGER_DOWN/MOTION/UP coordinates in WINDOW PIXELS plus a
// millisecond clock (SDL_GetTicks), then polls tick() once per frame. It
// emits:
//
//   LongPress — one finger held still >= 500 ms (MobileAdapter::onLongPress
//               maps it to a right-click; the platform layer owns the timer,
//               matching the adapter's documented contract).
//   Pinch     — two-finger distance ratio changes past a slop, emitted as
//               incremental ratio pulses (MobileAdapter::onPinch converts
//               each delta to a mouse-wheel zoom; the adapter lazily takes
//               the first event of a gesture as its baseline).
//
// Coords stay in the input domain (window px); the adapter applies its own
// display scale. Finger capacity mirrors the adapter's MAX_TOUCH_POINTS (8).

struct GestureEvent {
    enum class Kind { None, LongPress, Pinch };
    Kind  kind = Kind::None;
    float x = 0.0f;        // LongPress: press position / Pinch: centroid
    float y = 0.0f;
    float scale = 1.0f;    // Pinch: current distance / baseline distance
};

class GestureDetector {
public:
    static constexpr double kLongPressMs   = 500.0;
    static constexpr float  kMoveSlop      = 16.0f;   // px before a "hold" becomes a drag
    static constexpr float  kPinchInitial  = 0.08f;   // first ratio change that starts a pinch
    static constexpr float  kPinchStep     = 0.02f;   // later incremental pulse threshold
    static constexpr int    kMaxFingers    = 8;

    void reset();

    void onFingerDown(int id, float x, float y, double nowMs);
    void onFingerMove(int id, float x, float y, double nowMs);
    void onFingerUp(int id, double nowMs);

    // Poll for at most ONE gesture event (long-press fires exactly once per
    // press; pinch fires at most once per call).
    GestureEvent tick(double nowMs);

    int activeFingerCount() const;

private:
    struct Finger {
        bool    active = false;
        int     id = -1;
        float   x = 0.0f, y = 0.0f;
        double  downAtMs = 0.0;
        bool    moved = false;
    };

    void beginPinchBase(double nowMs);
    Finger* findFinger(int id);

    Finger m_fingers[kMaxFingers];
    int    m_active = 0;
    bool   m_longPressFired = false;
    float  m_pinchBaseDist = 0.0f;
    float  m_pinchLastRatio = 1.0f;
    bool   m_pinchStarted = false;
};

} // namespace Caesura
