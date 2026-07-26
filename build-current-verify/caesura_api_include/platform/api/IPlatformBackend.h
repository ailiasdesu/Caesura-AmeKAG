 #pragma once
#include <string>
#include <cstdint>

namespace Caesura {

// ---------------------------------------------------------------------------
// IPlatformBackend   Abstract platform backend interface
// ---------------------------------------------------------------------------
// Spec [0.4]: Concrete implementation wraps SDL3 operations  
// window creation, event polling, timer ticks, native handle queries.
// Registered in BackendRegistry alongside IRenderDevice and IAudioBackend.

class IPlatformBackend {
public:
    virtual ~IPlatformBackend() = default;

    // -- Lifecycle ---------------------------------------------------------
    virtual bool init(const char* title, int width, int height) = 0;
    virtual void shutdown() = 0;

    // -- Event polling -----------------------------------------------------
    // Returns true if an event was polled. The event is stored internally
    // or dispatched to registered callbacks.
    virtual bool pollEvent() = 0;

    // -- Mouse state query (replaces raw SDL_GetMouseState in engine) -----
    struct MouseState { float x = 0; float y = 0; bool leftDown = false; };
    virtual MouseState getMouseState() const = 0;

    // -- Timing ------------------------------------------------------------
    virtual uint64_t getTicksMs() const = 0;

    // -- Native window handle (for bgfx/other GPU surface creation) --------
    virtual void* getNativeWindowHandle() const = 0;

    // -- Window properties -------------------------------------------------
    virtual int getWindowWidth() const  = 0;
    virtual int getWindowHeight() const = 0;
    virtual void setFullscreen(bool fullscreen) = 0;

    // -- Window management --------------------------------------------------
    virtual void resizeWindow(int width, int height) = 0;

    // -- Backend identification --------------------------------------------
    virtual const char* getBackendName() const = 0;
};

} // namespace Caesura
