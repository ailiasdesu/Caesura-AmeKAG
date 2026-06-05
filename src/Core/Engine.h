#pragma once

#include "Core/IPlatformBackend.h"
#include "Core/DebugManager.h"
#include <memory>

namespace Caesura {

class IRenderDevice;
class IAudioBackend;
class LuaManager;
class InputRouter;

// -- Engine -- Top-level engine class --------------------------------------
// Creates platform backend first, then initializes all subsystems through
// BackendRegistry, and runs the main loop. All backend instances are
// stored in BackendRegistry and resolved by Lua bindings at call time.

class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool init(const char* title, int width, int height);
    void run();
    void quit();

    // Accessors (resolve from BackendRegistry)
    IRenderDevice& renderDevice();
    IAudioBackend& audio();
    IPlatformBackend& platform();
    LuaManager&   lua()           { return *m_lua; }
    InputRouter&  input()         { return *m_inputRouter; }

private:
    void processEvents();
    void render();
    void shutdown();

    int          m_width    = 1280;
    int          m_height   = 720;
    bool         m_running  = false;
    uint64_t     m_lastTick = 0;
    bool         m_shutdownComplete = false;

    // Owned backend instances (engine owns, BackendRegistry holds raw ptrs)
    std::unique_ptr<IRenderDevice>     m_renderDevice;
    std::unique_ptr<IAudioBackend>     m_audioBackend;
    std::unique_ptr<IPlatformBackend>  m_platformBackend;
    std::unique_ptr<LuaManager>        m_lua;
    std::unique_ptr<InputRouter>       m_inputRouter;
};

} // namespace Caesura
