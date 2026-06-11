// MobileAdapter implementation -- mobile platform stubs (P2 reserved).
// Spec [10.2.64]: All methods are placeholder stubs for future mobile port.
// No actual mobile SDK code -- just the interface wiring.
#include "MobileAdapter.h"
#include <cstring>
#include <cstdio>
#include <SDL3/SDL.h>

// Minimal Lua include -- we only need lua_State* for callbacks
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace Caesura {

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

    // Touch-to-mouse: inject SDL mouse button down event
    SDL_Event ev = {};
    ev.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    ev.button.x = x * m_displayScale;
    ev.button.y = y * m_displayScale;
    ev.button.button = SDL_BUTTON_LEFT;
    ev.button.clicks = 1;
    SDL_PushEvent(&ev);
}

void MobileAdapter::onFingerMotion(float x, float y, int fingerId) {
    if (fingerId >= 0 && fingerId < MAX_TOUCH_POINTS) {
        m_touchPoints[fingerId].x = x;
        m_touchPoints[fingerId].y = y;
    }

    // Touch-to-mouse: inject SDL mouse motion event
    SDL_Event ev = {};
    ev.type = SDL_EVENT_MOUSE_MOTION;
    ev.motion.x = x * m_displayScale;
    ev.motion.y = y * m_displayScale;
    SDL_PushEvent(&ev);
}

void MobileAdapter::onFingerUp(float x, float y, int fingerId) {
    if (fingerId >= 0 && fingerId < MAX_TOUCH_POINTS) {
        m_touchPoints[fingerId].active = false;
    }
    if (m_activeTouches > 0) m_activeTouches--;

    // Touch-to-mouse: inject SDL mouse button up event
    SDL_Event ev = {};
    ev.type = SDL_EVENT_MOUSE_BUTTON_UP;
    ev.button.x = x * m_displayScale;
    ev.button.y = y * m_displayScale;
    ev.button.button = SDL_BUTTON_LEFT;
    ev.button.clicks = 1;
    SDL_PushEvent(&ev);
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
    // Long press → right mouse button click
    SDL_Event ev = {};
    ev.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    ev.button.x = x * m_displayScale;
    ev.button.y = y * m_displayScale;
    ev.button.button = SDL_BUTTON_RIGHT;
    ev.button.clicks = 1;
    SDL_PushEvent(&ev);
    // Synthesize immediate button-up
    ev.type = SDL_EVENT_MOUSE_BUTTON_UP;
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
