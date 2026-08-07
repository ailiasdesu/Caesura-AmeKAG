// MobileAdapter implementation -- mobile platform adapter.
// Spec [10.2.64]: touch input mapping, lifecycle events, DPI scaling.
// Core mapping is implemented; native mobile SDK integration is not wired.
#include "MobileAdapter.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <SDL3/SDL.h>

// Minimal Lua include -- we only need lua_State* for callbacks
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace Caesura {

// Reject non-finite (NaN/Inf) coordinates from the OS layer before they can
// poison touch state or flow into injected SDL events / Lua globals.
static bool validCoord(float v) {
    return std::isfinite(v);
}

// ══════════════════════════════════════════════════════════════════════════
//  onOrientationChanged -- display orientation change (P7)
// ══════════════════════════════════════════════════════════════════════════
void MobileAdapter::onOrientationChanged(lua_State* L, const char* orientation) {
    if (!L || !orientation || !*orientation) return;
    lua_getglobal(L, "_G");
    lua_getfield(L, -1, "onOrientationChanged");
    if (lua_isfunction(L, -1)) {
        lua_pushstring(L, orientation);
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            lua_pop(L, 1);  // error object
        }
    } else {
        lua_pop(L, 1);  // non-function
    }
    lua_pop(L, 1);  // _G
}

// ══════════════════════════════════════════════════════════════════════════
//  onPause -- backgrounding
// ══════════════════════════════════════════════════════════════════════════
void MobileAdapter::onPause(lua_State* L) {
    m_paused = true;

    // TODO: Pause SoLoud audio engine here when mobile audio backend is wired.
    // For now, call Lua callback if available.
    if (L) {
        lua_getglobal(L, "_G");
        lua_getfield(L, -1, "onPause");
        if (lua_isfunction(L, -1)) {
            // On callback error Lua leaves the error object on the stack;
            // pop it so _G below stays balanced (error-path only).
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                lua_pop(L, 1);
            }
        } else {
            lua_pop(L, 1); // pop non-function value
        }
        lua_pop(L, 1); // pop _G
    }
}

// ══════════════════════════════════════════════════════════════════════════
//  onResume -- foregrounding
// ══════════════════════════════════════════════════════════════════════════
void MobileAdapter::onResume(lua_State* L, const std::string& savedData) {
    m_paused = false;

    // TODO: Resume SoLoud audio engine here when mobile audio backend is wired.
    if (L) {
        lua_getglobal(L, "_G");
        lua_getfield(L, -1, "onResume");
        if (lua_isfunction(L, -1)) {
            int result;
            if (!savedData.empty()) {
                lua_pushstring(L, savedData.c_str());
                result = lua_pcall(L, 1, 0, 0);
            } else {
                result = lua_pcall(L, 0, 0, 0);
            }
            // Pop the error object pushed by a failed callback so the
            // _G pop below keeps the stack balanced.
            if (result != LUA_OK) {
                lua_pop(L, 1);
            }
        } else {
            lua_pop(L, 1);
        }
        lua_pop(L, 1); // pop _G
    }
}

// ══════════════════════════════════════════════════════════════════════════
//  Touch → Mouse Mapping
// ══════════════════════════════════════════════════════════════════════════

void MobileAdapter::onFingerDown(float x, float y, int fingerId) {
    if (!validCoord(x) || !validCoord(y)) return; // non-finite input
    if (fingerId < 0 || fingerId >= MAX_TOUCH_POINTS) {
        return; // out of range -- ignore
    }
    if (m_touchPoints[fingerId].active) {
        // Finger already tracked (duplicate down): update position only.
        m_touchPoints[fingerId].x = x;
        m_touchPoints[fingerId].y = y;
        return;
    }
    m_touchPoints[fingerId] = TouchPoint{ x, y, fingerId, true };
    m_activeTouches++;

    // Touch-to-mouse: inject SDL mouse button down event
    SDL_Event ev = {};
    ev.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    ev.button.x = x * m_displayScale;
    ev.button.y = y * m_displayScale;
    ev.button.button = SDL_BUTTON_LEFT;
    ev.button.down = true;
    ev.button.clicks = 1;
    SDL_PushEvent(&ev);
}

void MobileAdapter::onFingerMotion(float x, float y, int fingerId) {
    if (!validCoord(x) || !validCoord(y)) return; // non-finite input
    if (fingerId < 0 || fingerId >= MAX_TOUCH_POINTS) {
        return; // out of range -- ignore
    }
    if (!m_touchPoints[fingerId].active) {
        return; // motion only valid for a tracked finger
    }
    m_touchPoints[fingerId].x = x;
    m_touchPoints[fingerId].y = y;

    // Touch-to-mouse: inject SDL mouse motion event
    SDL_Event ev = {};
    ev.type = SDL_EVENT_MOUSE_MOTION;
    ev.motion.x = x * m_displayScale;
    ev.motion.y = y * m_displayScale;
    SDL_PushEvent(&ev);
}

void MobileAdapter::onFingerUp(float x, float y, int fingerId) {
    // Note: rejecting non-finite up coordinates intentionally leaves the
    // finger tracked (asserted by the non-finite-inputs test) -- the
    // platform layer must send the up with valid coordinates.
    if (!validCoord(x) || !validCoord(y)) return; // non-finite input
    if (fingerId < 0 || fingerId >= MAX_TOUCH_POINTS) {
        return; // out of range -- ignore
    }
    if (!m_touchPoints[fingerId].active) {
        return; // finger was never down -- ignore (no underflow)
    }
    m_touchPoints[fingerId] = TouchPoint{ x, y, fingerId, false };
    if (m_activeTouches > 0) m_activeTouches--;

    // Touch-to-mouse: inject SDL mouse button up event
    SDL_Event ev = {};
    ev.type = SDL_EVENT_MOUSE_BUTTON_UP;
    ev.button.x = x * m_displayScale;
    ev.button.y = y * m_displayScale;
    ev.button.button = SDL_BUTTON_LEFT;
    ev.button.down = false;
    ev.button.clicks = 1;
    SDL_PushEvent(&ev);
}

// ══════════════════════════════════════════════════════════════════════════
//  Gestures
// ══════════════════════════════════════════════════════════════════════════

// Wheel delta emitted per unit of pinch scale change.
static constexpr float kPinchToWheelScale = 100.0f;

void MobileAdapter::onPinch(float centerX, float centerY, float scale) {
    if (!validCoord(centerX) || !validCoord(centerY) || !validCoord(scale)) {
        return; // non-finite input -- must not poison the scale baseline
    }
    if (m_lastPinchScale <= 0.0f) {
        // First event of a new pinch gesture: establish the baseline.
        m_lastPinchScale = scale;
        return;
    }
    const float delta = scale - m_lastPinchScale;
    m_lastPinchScale = scale;
    if (delta == 0.0f) return;

    // Map pinch scale delta to a vertical mouse-wheel (zoom) event.
    SDL_Event ev = {};
    ev.type = SDL_EVENT_MOUSE_WHEEL;
    ev.wheel.y = delta * kPinchToWheelScale;
    ev.wheel.mouse_x = centerX * m_displayScale;
    ev.wheel.mouse_y = centerY * m_displayScale;
    SDL_PushEvent(&ev);
}

void MobileAdapter::onLongPress(float x, float y) {
    if (!validCoord(x) || !validCoord(y)) return; // non-finite input
    // Long press → right mouse button click.
    // Note: press-duration tracking (>500ms) is done by the platform layer;
    // this adapter only maps the detected press to a right-click.
    SDL_Event ev = {};
    ev.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    ev.button.x = x * m_displayScale;
    ev.button.y = y * m_displayScale;
    ev.button.button = SDL_BUTTON_RIGHT;
    ev.button.down = true;
    ev.button.clicks = 1;
    SDL_PushEvent(&ev);
    // Synthesize immediate button-up
    ev.type = SDL_EVENT_MOUSE_BUTTON_UP;
    ev.button.down = false;
    SDL_PushEvent(&ev);
}

// ══════════════════════════════════════════════════════════════════════════
//  Display
// ══════════════════════════════════════════════════════════════════════════

float MobileAdapter::getDisplayScale() const {
    // Desktop: always 1.0.
    // Mobile: return actual DPI scale factor.
    return m_displayScale;
}

// ══════════════════════════════════════════════════════════════════════════
//  State
// ══════════════════════════════════════════════════════════════════════════

bool MobileAdapter::isFingerDown(int fingerId) const {
    if (fingerId < 0 || fingerId >= MAX_TOUCH_POINTS) return false;
    return m_touchPoints[fingerId].active;
}

} // namespace Caesura
