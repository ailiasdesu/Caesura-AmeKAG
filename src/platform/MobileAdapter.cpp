// MobileAdapter implementation -- mobile platform adapter.
// Spec [10.2.64]: touch input mapping, lifecycle events, DPI scaling.
// Core mapping is implemented; native mobile SDK integration is not wired.
#include "MobileAdapter.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <SDL3/SDL.h>

// ── Android JNI audio-focus bridge (t211 / Track M A2 JNI) ──────────────────
// The JNI natives below receive Android audio-focus changes from the host
// Activity (com.caesura.app.MainActivity). They run on the UI thread, so they
// NEVER touch engine services directly (SoLoud suspend/resume are
// CAESURA_ASSERT_MAIN_THREAD). Events are enqueued thread-safely and drained
// ON THE ENGINE/SDL THREAD by the composition root (paired Engine patch:
// setMobileNativeAudioFocusSink + mobileNativeDrainAudioFocus).
#if defined(__ANDROID__)
#include <jni.h>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>
#endif

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

// ── Gesture tracing (C6 收口) ─────────────────────────────────────────────
// Every gesture mapping below used to printf unconditionally. Gestures sit on
// a per-frame path (a pinch pulses every ~2% of travel and a three-finger hold
// re-arms on every touch sequence), so an unconditional printf floods stdout
// on a real device and costs a formatted write per pulse.
//
// SDL_LogDebug on SDL_LOG_CATEGORY_INPUT is used rather than the engine's
// DEBUG_* macros, deliberately:
//   * platform has 0/4 cross-module dependencies today (scripts/
//     count_coupling.py) and links only SDL3 + lua. DEBUG_* would pull in
//     CaesuraDebug, which requires editing cmake/CaesuraModules.cmake — a
//     shared coupling point outside this task's file set — for no gain here.
//   * SDL's documented defaults are app=info, assert=warn, test=verbose and
//     *=error (SDL_log.h), so INPUT-category DEBUG output is SILENT unless a
//     developer opts in — precisely the "no flood, still diagnosable" behavior
//     this fix wants.
//   * On Android these lines reach logcat instead of a swallowed stdout.
// Turn them on while debugging a device with:
//   SDL_SetLogPriority(SDL_LOG_CATEGORY_INPUT, SDL_LOG_PRIORITY_DEBUG);
#define MOBILE_GESTURE_TRACE(...) \
    SDL_LogDebug(SDL_LOG_CATEGORY_INPUT, __VA_ARGS__)

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

    // Audio suspend on backgrounding is wired at the composition root
    // (Engine::appLifecycleWatch -> IAudioBackend::suspend, round 29);
    // this adapter stays platform-pure and only notifies Lua.
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

    // Audio resume on foregrounding is wired at the composition root
    // (Engine::appLifecycleWatch -> IAudioBackend::resume, round 29).
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
//  onLowMemory / onTerminate -- unified lifecycle (Track P2)
// ══════════════════════════════════════════════════════════════════════════
static void invokeGlobalCallback(lua_State* L, const char* name) {
    if (!L) return;
    lua_getglobal(L, "_G");
    lua_getfield(L, -1, name);
    if (lua_isfunction(L, -1)) {
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            lua_pop(L, 1); // error object
        }
    } else {
        lua_pop(L, 1); // non-function
    }
    lua_pop(L, 1); // _G
}

void MobileAdapter::onLowMemory(lua_State* L) {
    invokeGlobalCallback(L, "onLowMemory");
}

void MobileAdapter::onTerminate(lua_State* L) {
    invokeGlobalCallback(L, "onTerminate");
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
    MOBILE_GESTURE_TRACE("[Mobile] Pinch -> wheel (%.0f, %.0f scale=%.3f)",
                         centerX * m_displayScale, centerY * m_displayScale, scale);
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
    MOBILE_GESTURE_TRACE("[Mobile] Long press -> right click (%.0f, %.0f)",
                         x * m_displayScale, y * m_displayScale);
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

void MobileAdapter::onTwoFingerTap(float centerX, float centerY) {
    if (!validCoord(centerX) || !validCoord(centerY)) return;
    MOBILE_GESTURE_TRACE("[Mobile] Two-finger tap -> right click (%.0f, %.0f)",
                         centerX * m_displayScale, centerY * m_displayScale);
    SDL_Event ev = {};
    ev.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    ev.button.x = centerX * m_displayScale;
    ev.button.y = centerY * m_displayScale;
    ev.button.button = SDL_BUTTON_RIGHT;
    ev.button.down = true;
    ev.button.clicks = 1;
    SDL_PushEvent(&ev);
    ev.type = SDL_EVENT_MOUSE_BUTTON_UP;
    ev.button.down = false;
    SDL_PushEvent(&ev);
}

void MobileAdapter::onThreeFingerHold(float centerX, float centerY) {
    if (!validCoord(centerX) || !validCoord(centerY)) return;
    MOBILE_GESTURE_TRACE("[Mobile] Three-finger hold -> skip toggle (%.0f, %.0f)",
                         centerX * m_displayScale, centerY * m_displayScale);
    SDL_Event ev = {};
    ev.type = SDL_EVENT_KEY_DOWN;
    ev.key.key = SDLK_LCTRL;
    ev.key.down = true;
    ev.key.repeat = false;
    SDL_PushEvent(&ev);
}

// WIRED (t109): Engine.cpp's key-down handler routes SDLK_SPACE to the Lua
// hook _KAG_onKeySpace (kag_demo_entry.lua), which toggles the message-layer
// visibility -- mirroring the web gesture (web/main.mjs onSwipeDown).
void MobileAdapter::onSwipeDown(float startX, float startY, float endX, float endY) {
    if (!validCoord(startX) || !validCoord(startY) || !validCoord(endX) || !validCoord(endY)) return;
    MOBILE_GESTURE_TRACE("[Mobile] SwipeDown -> SPACE (%.0f, %.0f -> %.0f, %.0f)",
                         startX * m_displayScale, startY * m_displayScale,
                         endX * m_displayScale, endY * m_displayScale);
    SDL_Event ev = {};
    ev.type = SDL_EVENT_KEY_DOWN;
    ev.key.key = SDLK_SPACE;
    ev.key.down = true;
    ev.key.repeat = false;
    SDL_PushEvent(&ev);
}

// WIRED (t109): Engine.cpp routes SDLK_PAGEUP to the Lua hook
// _KAG_onKeyPageUp (kag_demo_entry.lua), which opens the backlog/history
// overlay -- mirroring the web gesture (web/main.mjs onSwipeUp).
void MobileAdapter::onSwipeUp(float startX, float startY, float endX, float endY) {
    if (!validCoord(startX) || !validCoord(startY) || !validCoord(endX) || !validCoord(endY)) return;
    MOBILE_GESTURE_TRACE("[Mobile] SwipeUp -> PAGEUP (%.0f, %.0f -> %.0f, %.0f)",
                         startX * m_displayScale, startY * m_displayScale,
                         endX * m_displayScale, endY * m_displayScale);
    SDL_Event ev = {};
    ev.type = SDL_EVENT_KEY_DOWN;
    ev.key.key = SDLK_PAGEUP;
    ev.key.down = true;
    ev.key.repeat = false;
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

#if defined(__ANDROID__)

// ══════════════════════════════════════════════════════════════════════════
//  JNI audio-focus bridge (Track M A2 / t211)
// ══════════════════════════════════════════════════════════════════════════
// Android's audio-focus arbitration is NOT surfaced by SDL3, so the host
// Activity (com.caesura.app.MainActivity) forwards AudioManager changes here.
//
// Threading contract: the AudioManager listener fires on the UI thread, while
// engine services must only be touched on the engine/SDL thread (SoLoud
// suspend/resume are CAESURA_ASSERT_MAIN_THREAD). These natives therefore
// ONLY enqueue; the composition root drains on its SDL-thread loop via the
// paired Engine patch (setMobileNativeAudioFocusSink + drain call per frame).
// The sink receives raw Android focus codes; Engine maps them onto
// AudioFocusEvent and posts into IAudioFocusService.
namespace {

std::mutex g_focusMutex;
std::vector<int> g_pendingFocus;
std::function<void(int)> g_focusSink;

} // namespace

// Install the drain sink (called from the composition root, engine thread).
void setMobileNativeAudioFocusSink(std::function<void(int)> sink) {
    std::lock_guard<std::mutex> lock(g_focusMutex);
    g_focusSink = std::move(sink);
}

// Drain pending focus changes on the engine/SDL thread (called once per frame
// by the composition root). No-op when nothing is pending or no sink is set.
void mobileNativeDrainAudioFocus() {
    std::function<void(int)> sink;
    std::vector<int> pending;
    {
        std::lock_guard<std::mutex> lock(g_focusMutex);
        if (g_pendingFocus.empty() || !g_focusSink) return;
        pending.swap(g_pendingFocus);
        sink = g_focusSink;
    }
    for (int code : pending) {
        sink(code);
    }
}

// Android AudioManager constants (the platform contract; values from the
// android.media.AudioManager API): AUDIOFOCUS_GAIN=1, GAIN_TRANSIENT=2,
// GAIN_TRANSIENT_MAY_DUCK=3, AUDIOFOCUS_LOSS=-1, LOSS_TRANSIENT=-2,
// LOSS_TRANSIENT_CAN_DUCK=-3. Kept as raw ints: MainActivity forwards the
// AudioManager change, Engine owns the AudioFocusEvent mapping.
extern "C" JNIEXPORT void JNICALL
Java_com_caesura_app_MainActivity_nativeOnAudioFocusChanged(
    JNIEnv*, jobject, jint code) {
    std::lock_guard<std::mutex> lock(g_focusMutex);
    g_pendingFocus.push_back(static_cast<int>(code));
}

#endif // __ANDROID__

} // namespace Caesura
