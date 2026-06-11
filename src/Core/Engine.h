#pragma once

#include "Core/EngineConfig.h"
#include "Core/IPlatformBackend.h"
#include <memory>
#include <thread>
#include <cassert>

namespace Caesura {

class IRenderDevice;
class IAudioBackend;
class LuaManager;
class InputRouter;
class GpuMonitor;
class VideoPlayer;
class DebugManager;
class IMiniGameBackend;
class IAnimationBackend;
class ISteamBackend;

// -- Engine -- Top-level engine class --------------------------------------
// Creates platform backend first, then initializes all subsystems through
// BackendRegistry, and runs the main loop. All backend instances are
// stored in BackendRegistry and resolved by Lua bindings at call time.

class Engine {
public:
    Engine(const EngineConfig& config);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool init();
    void run();
    void runRpc();  // stdin/stdout JSON-RPC loop

    bool isHeadless() const { return m_config.headless; }
    bool isEditorMode() const { return m_config.editorMode; }
    void renderOneFrame();  // render single frame (for editor/RPC screenshot)
    std::string captureFrameForRpc(int w, int h);
    void quit();

    // Accessors (resolve from BackendRegistry)
    IRenderDevice& renderDevice();
    IAudioBackend& audio();
    IPlatformBackend& platform();
    IMiniGameBackend& miniGame() { return *m_miniGameBackend; }
    IAnimationBackend& animation() { return *m_animationBackend; }
    LuaManager&   lua()           { return *m_lua; }
    InputRouter&  input()         { return *m_inputRouter; }
    GpuMonitor&   gpuMonitor()    { return *m_gpuMonitor; }
    VideoPlayer&  videoPlayer()   { return *m_videoPlayer; }

    const EngineConfig& config() const { return m_config; }

    // -- Thread safety: main thread id for debug assertions
    static std::thread::id s_mainThreadId;

private:
    void processEvents();
    void render();

    // Auto-save
    void setAutoSaveInterval(float seconds) { m_autoSaveInterval = seconds; }
    float autoSaveInterval() const { return m_autoSaveInterval; }
    void triggerAutoSave();
    void quicksave();
    void quickload();

    void handleFatalError(const char* context, const char* luaError);
    void shutdown();

    bool         m_running  = false;
    uint64_t     m_lastTick = 0;
    bool         m_shutdownComplete = false;

    // Audio voice-complete tracking (no polling -- detects edge in main loop)
    bool         m_audioVoiceWasPlaying = false;

    // -- Phase G8-U1: Lua memory management
    int  m_gcFrameCounter = 0;
    static void* luaAllocHook(void* ud, void* ptr, size_t osize, size_t nsize);

    // Owned backend instances (engine owns, BackendRegistry holds raw ptrs)
    std::unique_ptr<IRenderDevice>     m_renderDevice;
    std::unique_ptr<IAudioBackend>     m_audioBackend;
    std::unique_ptr<IPlatformBackend>  m_platformBackend;
    std::unique_ptr<LuaManager>        m_lua;
    std::unique_ptr<InputRouter>       m_inputRouter;
    std::unique_ptr<GpuMonitor>        m_gpuMonitor;
    std::unique_ptr<IMiniGameBackend> m_miniGameBackend;
    float m_autoSaveInterval = 0.0f;
    float m_autoSaveTimer = 0.0f;
    float m_frameTime = 0.0f;
    std::unique_ptr<IAnimationBackend>  m_animationBackend;
    std::unique_ptr<ISteamBackend>      m_steamBackend;
    std::unique_ptr<VideoPlayer>       m_videoPlayer;
    EngineConfig m_config;    // stored config, populated by ctor
    // HotReload is a singleton, accessed via HotReload::instance()
};

} // namespace Caesura

// -- Thread safety assertion macro (Debug only) --
#ifndef NDEBUG
#define CAESURA_ASSERT_MAIN_THREAD() \
    assert(std::this_thread::get_id() == ::Caesura::Engine::s_mainThreadId)
#define CAESURA_ASSERT_NOT_MAIN_THREAD() \
    assert(std::this_thread::get_id() != ::Caesura::Engine::s_mainThreadId)
#else
#define CAESURA_ASSERT_MAIN_THREAD() ((void)0)
#define CAESURA_ASSERT_NOT_MAIN_THREAD() ((void)0)
#endif
