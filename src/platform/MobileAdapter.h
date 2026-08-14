// MobileAdapter -- Mobile platform adapter.
// Spec [10.2.64]: Touch input mapping, lifecycle events, DPI scaling.
// Core mapping is implemented and unit-tested (see class doc); native mobile
// platform integration is not wired into the Engine yet.
// Namespace: Caesura (consistent with engine layering).
#pragma once
#include "api/IMobileAdapter.h"
#include <cmath>
#include <cstdint>
#include <string>

// Forward declaration for Lua state (avoid full include in header)
struct lua_State;

namespace Caesura {

/// Touch point data for finger events
struct TouchPoint {
    float x = 0.0f;
    float y = 0.0f;
    int   fingerId = 0;
    bool  active = false;
};

/// Mobile platform adapter -- lifecycle + touch → input mapping.
/// Core mapping (touch → SDL mouse/wheel events, DPI scale, pause/resume
/// Lua callbacks) is implemented and unit-tested. Native mobile platform
/// integration (real OS lifecycle hooks) is NOT wired into the Engine yet --
/// the platform layer must call these methods. Audio suspend/resume on
/// backgrounding is handled at the composition root
/// (Engine::appLifecycleWatch -> IAudioBackend, round 29).
class MobileAdapter : public IMobileAdapter {
public:
    MobileAdapter() = default;
    ~MobileAdapter() override = default;

    // ── Lifecycle ──────────────────────────────────────────────────────

    /// Called when app goes to background.
    /// Pauses SoLoud audio and invokes Lua _G.onPause().
    void onPause(lua_State* L) override;

    /// Called when app returns to foreground.
    /// Resumes audio and invokes Lua _G.onResume(savedData).
    void onResume(lua_State* L, const std::string& savedData = "") override;

    /// Display orientation change -> Lua _G.onOrientationChanged(name).
    void onOrientationChanged(lua_State* L, const char* orientation) override;

    // ── Touch → Mouse Mapping ──────────────────────────────────────────

    /// Single finger down -- maps to left mouse button press at (x, y).
    void onFingerDown(float x, float y, int fingerId = 0) override;

    /// Finger moved -- maps to mouse motion at (x, y).
    void onFingerMotion(float x, float y, int fingerId = 0) override;

    /// Finger lifted -- maps to left mouse button release at (x, y).
    void onFingerUp(float x, float y, int fingerId = 0) override;

    // ── Gesture Input ──────────────────────────────────────────────────

    /// Pinch gesture -- zoom in/out.
    /// `scale` is the cumulative gesture scale (starts at 1.0f). Each call
    /// maps the scale delta to a vertical mouse-wheel event (zoom). Call
    /// `resetPinch()` when the gesture ends (all fingers lifted).
    void onPinch(float centerX, float centerY, float scale) override;

    /// End the active pinch gesture (resets the scale baseline).
    void resetPinch() override { m_lastPinchScale = 0.0f; }

    /// Current pinch scale baseline (0 = no active pinch gesture).
    float getLastPinchScale() const override { return m_lastPinchScale; }

    /// Long press -- maps to right mouse button click.
    /// Press-duration tracking (>500ms) is the platform layer's
    /// responsibility; this callback fires once when the press is detected.
    void onLongPress(float x, float y) override;

    // ── Display ────────────────────────────────────────────────────────

    /// Get display scale factor (DPI-based).
    /// Desktop returns 1.0; mobile returns actual DPI scale.
    float getDisplayScale() const override;

    /// Set display scale for testing. Non-finite values are rejected.
    void setDisplayScale(float scale) override {
        m_displayScale = std::isfinite(scale) ? scale : 1.0f;
    }

    // ── State ──────────────────────────────────────────────────────────

    /// Check if currently in background / paused.
    bool isPaused() const override { return m_paused; }

    /// Get the active touch point count.
    int activeTouchCount() const override { return m_activeTouches; }

    /// Check if a specific finger is currently down.
    bool isFingerDown(int fingerId) const override;

private:
    bool  m_paused = false;
    float m_displayScale = 1.0f;
    float m_lastPinchScale = 0.0f;
    int   m_activeTouches = 0;

    /// Last known positions per finger (up to 10 simultaneous touches)
    static constexpr int MAX_TOUCH_POINTS = 10;
    TouchPoint m_touchPoints[MAX_TOUCH_POINTS];
};

} // namespace Caesura
