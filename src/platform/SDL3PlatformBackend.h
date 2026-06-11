#pragma once
#include "api/IPlatformBackend.h"
#include <SDL3/SDL.h>

namespace Caesura {

// -- SDL3 concrete implementation of IPlatformBackend -----------------------
// Wraps all SDL3 operations (window, events, timing, native handles)
// behind the abstract interface so Lua never touches SDL3 directly.

class SDL3PlatformBackend : public IPlatformBackend {
public:
    SDL3PlatformBackend() = default;
    ~SDL3PlatformBackend() override;

    SDL3PlatformBackend(const SDL3PlatformBackend&) = delete;
    SDL3PlatformBackend& operator=(const SDL3PlatformBackend&) = delete;

    // -- IPlatformBackend --------------------------------------------------
    bool init(const char* title, int width, int height) override;
    void shutdown() override;

    bool pollEvent() override;
    uint64_t getTicksMs() const override;
    MouseState getMouseState() const override;

    void* getNativeWindowHandle() const override;
    int getWindowWidth() const override  { return m_width; }
    int getWindowHeight() const override { return m_height; }
    void setFullscreen(bool fullscreen) override;
    void resizeWindow(int width, int height) override;

    const char* getBackendName() const override { return "SDL3"; }

    // -- SDL3-specific accessors (for C++ engine internals) -----------------
    SDL_Window* window()     const { return m_window; }
    SDL_Event&  lastEvent()        { return m_lastEvent; }

private:
    SDL_Window* m_window = nullptr;
    int  m_width  = 1280;
    int  m_height = 720;
    bool m_initialized = false;
    SDL_Event m_lastEvent;
};

} // namespace Caesura
