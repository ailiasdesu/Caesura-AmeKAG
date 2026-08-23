#pragma once

#include <cstdint>

namespace Caesura {

// ---------------------------------------------------------------------------
// ILifecycleService   Unified app-lifecycle event model (Track P2)
// ---------------------------------------------------------------------------
// One uniform event feed for desktop (SDL3 app-lifecycle events), Android
// (JNI onPause/onResume/onLowMemory) and iOS (UIApplication notifications).
// Consumers register once as ILifecycleListener — no platform ifdefs.
//
// Event sources MUST deliver on the engine/main thread (or marshal there):
// listeners may touch Lua. The desktop source today is
// Engine::appLifecycleWatch (SDL3 event watch, main thread).

enum class LifecycleEvent : uint8_t {
    Pause = 0,        // leaving the current focus (mobile: app goes to background)
    Resume = 1,       // returning to focus
    Background = 2,   // fully backgrounded (no visible surface)
    Foreground = 3,   // foregrounded again
    LowMemory = 4,    // OS memory pressure — free caches/textures if possible
    Terminate = 5,    // OS about to terminate the process
};

class ILifecycleListener {
public:
    virtual ~ILifecycleListener() = default;
    virtual void onLifecycleEvent(LifecycleEvent event) = 0;
};

class ILifecycleService {
public:
    virtual ~ILifecycleService() = default;

    // Register a listener (duplicates are ignored). Order of registration
    // is the dispatch order.
    virtual void addListener(ILifecycleListener* listener) = 0;

    // Remove a listener; removing unknown / already-removed is a safe no-op.
    virtual void removeListener(ILifecycleListener* listener) = 0;

    // Deliver an event to every listener (caller-thread dispatch).
    virtual void post(LifecycleEvent event) = 0;
};

} // namespace Caesura
