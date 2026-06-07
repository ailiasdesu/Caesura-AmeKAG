// MobileAdapter implementation -- mobile platform stubs (P2 reserved).
// Spec [10.2.64]: All methods are placeholder stubs for future mobile port.
// No actual mobile SDK code -- just the interface wiring.
#include "MobileAdapter.h"
#include <cstring>
#include <cstdio>

// Minimal Lua include -- we only need lua_State* for callbacks
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace caesura::platform {

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
            lua_pcall(L, 0, 0, 0);
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
            if (!savedData.empty()) {
                lua_pushstring(L, savedData.c_str());
                lua_pcall(L, 1, 0, 0);
            } else {
                lua_pcall(L, 0, 0, 0);
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
    if (fingerId >= 0 && fingerId < MAX_TOUCH_POINTS) {
        m_touchPoints[fingerId] = TouchPoint{ x, y, fingerId, true };
    }
    m_activeTouches++;

    // TODO: Inject SDL mouse button down event at scaled (x, y).
    // For mobile port: SDL_PushEvent with SDL_MOUSEBUTTONDOWN.
    (void)x;
    (void)y;
}

void MobileAdapter::onFingerMotion(float x, float y, int fingerId) {
    if (fingerId >= 0 && fingerId < MAX_TOUCH_POINTS) {
        m_touchPoints[fingerId].x = x;
        m_touchPoints[fingerId].y = y;
    }

    // TODO: Inject SDL mouse motion event at scaled (x, y).
    (void)x;
    (void)y;
}

void MobileAdapter::onFingerUp(float x, float y, int fingerId) {
    if (fingerId >= 0 && fingerId < MAX_TOUCH_POINTS) {
        m_touchPoints[fingerId].active = false;
    }
    if (m_activeTouches > 0) m_activeTouches--;

    // TODO: Inject SDL mouse button up event at scaled (x, y).
    (void)x;
    (void)y;
}

// ══════════════════════════════════════════════════════════════════════════
//  Gestures (reserved)
// ══════════════════════════════════════════════════════════════════════════

void MobileAdapter::onPinch(float centerX, float centerY, float scale) {
    // Reserved for future pinch-to-zoom implementation.
    (void)centerX;
    (void)centerY;
    (void)scale;
}

void MobileAdapter::onLongPress(float x, float y) {
    // TODO: Inject SDL right mouse button click event at scaled (x, y).
    (void)x;
    (void)y;
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

} // namespace caesura::platform
