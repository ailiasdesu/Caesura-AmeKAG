// MobileAdapter -- Mobile platform interface stubs (P2 reserved).
// Spec [10.2.64]: Touch input mapping, lifecycle events, DPI scaling.
// All methods are stubs for future mobile port -- no actual mobile code.
// Namespace: caesura::platform (consistent with engine layering).
#pragma once
#include <cstdint>
#include <string>

// Forward declaration for Lua state (avoid full include in header)
struct lua_State;

namespace caesura::platform {

/// Touch point data for finger events
struct TouchPoint {
    float x = 0.0f;
    float y = 0.0f;
    int   fingerId = 0;
    bool  active = false;
};

/// Mobile platform adapter -- lifecycle + touch → input mapping.
/// P2 reserved: all implementations are placeholder stubs.
class MobileAdapter {
public:
    MobileAdapter() = default;
    ~MobileAdapter() = default;

    // ── Lifecycle ──────────────────────────────────────────────────────

    /// Called when app goes to background.
    /// Pauses SoLoud audio and invokes Lua _G.onPause().
    void onPause(lua_State* L);

    /// Called when app returns to foreground.
    /// Resumes audio and invokes Lua _G.onResume(savedData).
    void onResume(lua_State* L, const std::string& savedData = "");

    // ── Touch → Mouse Mapping ──────────────────────────────────────────

    /// Single finger down -- maps to left mouse button press at (x, y).
    void onFingerDown(float x, float y, int fingerId = 0);

    /// Finger moved -- maps to mouse motion at (x, y).
    void onFingerMotion(float x, float y, int fingerId = 0);

    /// Finger lifted -- maps to left mouse button release at (x, y).
    void onFingerUp(float x, float y, int fingerId = 0);

    // ── Gesture Input (reserved) ───────────────────────────────────────

    /// Pinch gesture -- zoom in/out (reserved for future).
    void onPinch(float centerX, float centerY, float scale);

    /// Long press > 500ms -- maps to right mouse button click.
    void onLongPress(float x, float y);

    // ── Display ────────────────────────────────────────────────────────

    /// Get display scale factor (DPI-based).
    /// Desktop returns 1.0; mobile returns actual DPI scale.
    float getDisplayScale() const;

    /// Set display scale for testing.
    void setDisplayScale(float scale) { m_displayScale = scale; }

    // ── State ──────────────────────────────────────────────────────────

    /// Check if currently in background / paused.
    bool isPaused() const { return m_paused; }

    /// Get the active touch point count.
    int activeTouchCount() const { return m_activeTouches; }

    /// Check if a specific finger is currently down.
    bool isFingerDown(int fingerId) const;

private:
    bool  m_paused = false;
    float m_displayScale = 1.0f;
    int   m_activeTouches = 0;

    /// Last known positions per finger (up to 10 simultaneous touches)
    static constexpr int MAX_TOUCH_POINTS = 10;
    TouchPoint m_touchPoints[MAX_TOUCH_POINTS];
};

} // namespace caesura::platform
