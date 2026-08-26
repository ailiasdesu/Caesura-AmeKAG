// IMobileAdapter - pure virtual interface for mobile platform adaptation
// Concrete: platform/MobileAdapter. Pattern: module api/ directory.
// Lifecycle callbacks + touch -> input mapping (SDL event injection).
#pragma once
#include <string>

struct lua_State;

namespace Caesura {

class IMobileAdapter {
public:
    virtual ~IMobileAdapter() = default;

    // -- Lifecycle ---------------------------------------------------------
    // Called when the app goes to background: marks paused and invokes
    // Lua _G.onPause() when a Lua state is supplied.
    virtual void onPause(lua_State* L) = 0;

    // Called when the app returns to foreground: resumes and invokes
    // Lua _G.onResume(savedData) when a Lua state is supplied.
    virtual void onResume(lua_State* L, const std::string& savedData = "") = 0;

    // Unified lifecycle (Track P2): OS memory pressure -> _G.onLowMemory();
    // OS termination notice -> _G.onTerminate(). Both degrade to a safe
    // no-op without Lua and swallow callback errors (balanced stack).
    virtual void onLowMemory(lua_State* L) = 0;
    virtual void onTerminate(lua_State* L) = 0;

    // -- Touch -> mouse mapping --------------------------------------------
    virtual void onFingerDown(float x, float y, int fingerId) = 0;
    virtual void onFingerMotion(float x, float y, int fingerId) = 0;
    virtual void onFingerUp(float x, float y, int fingerId) = 0;

    // -- Gestures ----------------------------------------------------------
    virtual void onPinch(float centerX, float centerY, float scale) = 0;
    virtual void resetPinch() = 0;
    virtual float getLastPinchScale() const = 0;
    virtual void onLongPress(float x, float y) = 0;
    virtual void onTwoFingerTap(float centerX, float centerY) = 0;
    virtual void onThreeFingerHold(float centerX, float centerY) = 0;
    virtual void onSwipeDown(float startX, float startY, float endX, float endY) = 0;
    virtual void onSwipeUp(float startX, float startY, float endX, float endY) = 0;

    // -- Display -----------------------------------------------------------
    virtual float getDisplayScale() const = 0;
    virtual void setDisplayScale(float scale) = 0;
    // Orientation change (P7): SDL_EVENT_DISPLAY_ORIENTATION -> Lua
    // _G.onOrientationChanged("portrait"|"landscape"|...) when registered.
    virtual void onOrientationChanged(lua_State* L, const char* orientation) = 0;

    // -- State -------------------------------------------------------------
    virtual bool isPaused() const = 0;
    virtual int activeTouchCount() const = 0;
    virtual bool isFingerDown(int fingerId) const = 0;
};

} // namespace Caesura
