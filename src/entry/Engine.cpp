extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include "Engine.h"
#include "../input/InputRouter.h"
#include "../di/BackendRegistry.h"
#include "../debug/DebugManager.h"
#include "ErrorUI.h"
#include "../audio/api/IAudioBackend.h"
#include "../di/api/ITextureBudget.h"
#include "../di/api/ISandboxQuota.h"
#include "../render/api/IRenderDevice.h"
#include "../render/api/ITextureManager.h"
#include "../render/api/ILayerManager.h"
#include "../render/VideoPlayer.h"
#include "../render/api/IGpuMonitor.h"
#include "../render/api/IParticleSystem.h"
#include "../minigame/api/IMiniGameBackend.h"
#include "../live2d/api/IAnimationBackend.h"
#include "../script/vm/LuaManager.h"
#include "../debug/HotReload.h"
#include "../debug/DebugProtocol.h"
#include "../job/api/IJobSystem.h"
#include "../resource/api/IAsyncLoader.h"
#include "../resource/AssetManager.h"
#include "../resource/api/IResourceGenerationTracker.h"
#include "../steam/api/ISteamBackend.h"
#include "../script/bindings/SteamBinding.h"
#include "../script/bindings/VFXBinding.h"
#include "../storage/api/ISaveManager.h"
#include "../archive/api/ICryptoEngine.h"
#include <SDL3/SDL.h>
#include <thread>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <vector>
#include <cmath>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <utility>


namespace Caesura {

namespace {

constexpr const char* kDebugPausedGlobal = "_CAESURA_DEBUG_PAUSED";
constexpr const char* kDebugPauseProbeGlobal = "_CAESURA_DEBUG_IS_PAUSED";

int debugPauseProbe(lua_State* L) {
    auto* protocol = static_cast<DebugProtocol*>(
        lua_touserdata(L, lua_upvalueindex(1)));
    lua_pushboolean(L, protocol && protocol->isDebugActive() ? 1 : 0);
    return 1;
}

void installDebugPauseProbe(lua_State* L, DebugProtocol* protocol) {
    if (!L) return;
    lua_pushlightuserdata(L, protocol);
    lua_pushcclosure(L, debugPauseProbe, 1);
    lua_setglobal(L, kDebugPauseProbeGlobal);
    lua_pushboolean(L, 0);
    lua_setglobal(L, kDebugPausedGlobal);
}

void removeDebugPauseProbe(lua_State* L) {
    if (!L) return;
    lua_pushnil(L);
    lua_setglobal(L, kDebugPauseProbeGlobal);
    lua_pushboolean(L, 0);
    lua_setglobal(L, kDebugPausedGlobal);
}

} // namespace

// Factory for GpuMonitor (defined in Engine_Gpu.cpp 鈥?F1)
std::unique_ptr<IGpuMonitor> createGpuMonitor(bool headless);
std::unique_ptr<ISteamBackend> createDefaultSteamIntegration();
std::unique_ptr<IAudioBackend> createHeadlessAudioBackend();
std::unique_ptr<IRenderDevice> createHeadlessRenderDevice();
std::unique_ptr<IPlatformBackend> createHeadlessPlatformBackend();
std::unique_ptr<IMiniGameBackend> createFallbackMiniGameBackend();
std::unique_ptr<IParticleSystem> createParticleSystem();
std::unique_ptr<IResourceGenerationTracker> createResourceGenerationTracker();
std::unique_ptr<ITextureBudget> createTextureBudget();
std::unique_ptr<ITextureManager> createTextureManager();
std::unique_ptr<ILayerManager> createLayerManager(IRenderDevice* renderDevice);
std::unique_ptr<ISandboxQuota> createSandboxQuota();
std::unique_ptr<AssetManager> createAssetManager();
std::unique_ptr<IAsyncLoader> createAsyncLoader(AssetManager* assetManager);
std::unique_ptr<IJobSystem> createJobSystem();
std::unique_ptr<ISaveManager> createSaveManager();
std::unique_ptr<carc::ICryptoEngine> createCryptoEngine();
std::unique_ptr<IAnimationBackend> createDefaultAnimationBackend();
std::unique_ptr<IAnimationBackend> createFallbackAnimationBackend();
void attachRenderDeviceToAnimationBackend(IAnimationBackend* animationBackend,
                                           IRenderDevice* renderDevice);
void registerDefaultAssetProviders(AssetManager& assetManager);
void registerEngineLuaRegistryServices(lua_State* L,
                                       IInputRouter* inputRouter,
                                       IVideoPlayer* videoPlayer,
                                       IParticleSystem* particleSystem,
                                       ITextureManager* textureManager,
                                       IAsyncLoader* asyncLoader);
void registerMiniGameLuaRegistryService(lua_State* L,
                                         IMiniGameBackend* miniGameBackend);

EngineConfig::~EngineConfig() {
    delete steam;
    delete animation;
    delete miniGame;
    delete sandboxQuota;
    delete layerManager;
    delete videoPlayer;
    delete gpuMonitor;
    delete inputRouter;
    delete lua;
    delete platform;
    delete audio;
    delete render;
}


// -- Phase G8-U1: lua_Alloc hook for per-allocation memory check ---------------
// Lua 5.4 forbids calling lua_gc inside the allocator (stack overflow).
// Memory budget is enforced per-frame in Engine::run() instead.
static void* s_luaAllocFn(void* ud, void* ptr, size_t osize, size_t nsize) {
    // Lua 5.4 forbids calling lua_gc inside the allocator (C stack overflow).
    if (nsize == 0) { free(ptr); return nullptr; }
    return realloc(ptr, nsize);
}

Engine::Engine(EngineConfig&& config)
    : m_config(std::move(config))
    , m_renderDevice(std::exchange(m_config.render, nullptr))
    , m_audioBackend(std::exchange(m_config.audio, nullptr))
    , m_platformBackend(std::exchange(m_config.platform, nullptr))
    , m_lua(std::exchange(m_config.lua, nullptr))
    , m_hotReload(std::make_unique<HotReload>())
    , m_inputRouter(std::exchange(m_config.inputRouter, nullptr))
    , m_gpuMonitor(std::exchange(m_config.gpuMonitor, nullptr))
    , m_miniGameBackend(std::exchange(m_config.miniGame, nullptr))
    , m_animationBackend(std::exchange(m_config.animation, nullptr))
    , m_steamBackend(std::exchange(m_config.steam, nullptr))
    , m_videoPlayer(std::exchange(m_config.videoPlayer, nullptr))
    , m_layerManager(std::exchange(m_config.layerManager, nullptr))
    , m_sandboxQuota(std::exchange(m_config.sandboxQuota, nullptr))
{
    // Acquire every injected pointer before any default allocation can throw.
    if (!m_lua) m_lua = std::make_unique<LuaManager>();
    if (!m_inputRouter) m_inputRouter = std::make_unique<InputRouter>();
    if (!m_videoPlayer) m_videoPlayer = std::make_unique<VideoPlayer>();
    if (!m_steamBackend) m_steamBackend = createDefaultSteamIntegration();
    if (!m_sandboxQuota) m_sandboxQuota = createSandboxQuota();
}

Engine::~Engine() {
    if (!m_shutdownComplete) shutdown();
}

void Engine::requireInitialized() const {
    if (!m_initialized) {
        throw std::logic_error("Engine service access requires a successful init()");
    }
}

IRenderDevice& Engine::renderDevice() { requireInitialized(); return *m_renderDevice; }
IAudioBackend& Engine::audio() { requireInitialized(); return *m_audioBackend; }
IPlatformBackend& Engine::platform() { requireInitialized(); return *m_platformBackend; }

bool Engine::init() {
    if (m_initAttempted) {
        fprintf(stderr, "[Engine] init() may only be called once per Engine instance.\n");
        return false;
    }
    m_initAttempted = true;

    const bool useHeadlessDefaults = m_config.headless && !m_config.editorMode;
    if (useHeadlessDefaults && !m_audioBackend) {
        m_audioBackend = createHeadlessAudioBackend();
    }
    if (useHeadlessDefaults && !m_renderDevice) {
        m_renderDevice = createHeadlessRenderDevice();
    }
    if (useHeadlessDefaults && !m_platformBackend) {
        m_platformBackend = createHeadlessPlatformBackend();
    }
    if (useHeadlessDefaults && !m_miniGameBackend) {
        m_miniGameBackend = createFallbackMiniGameBackend();
    }
    if (!m_layerManager) m_layerManager = createLayerManager(m_renderDevice.get());
    if (!m_textureBudget) m_textureBudget = createTextureBudget();
    if (!m_textureManager) m_textureManager = createTextureManager();
    if (!m_assetManager) m_assetManager = createAssetManager();
    if (!m_asyncLoader) m_asyncLoader = createAsyncLoader(m_assetManager.get());
    if (!m_jobSystem) m_jobSystem = createJobSystem();
    if (!m_saveManager) m_saveManager = createSaveManager();
    if (!m_cryptoEngine) m_cryptoEngine = createCryptoEngine();

    detail::g_mainThreadId = std::this_thread::get_id();

    if (m_config.editorMode) {
        fprintf(stderr, "[Engine] Running in EDITOR mode (hidden window + GPU)\n");
    }
    if (m_config.headless) {
        fprintf(stderr, "[Engine] Running in HEADLESS mode (no window, no GPU)\n");
    }

    m_debugInitialized = DebugManager::instance().init("logs");
    if (!m_debugInitialized) {
        fprintf(stderr, "[Engine] DebugManager init failed - continuing.\n");
    }

    // T2: Phase-split initialization
    if (!initPlatformPhase()) { shutdown(); return false; }
    if (!initScriptingPhase()) { shutdown(); return false; }
    if (!initAssetPhase()) { shutdown(); return false; }
    if (!initOptionalPhase()) {
        fprintf(stderr, "[Engine] Optional subsystems init had issues (non-fatal).\n");
    }

    DEBUG_INFO(SubSys::Engine, ErrCode::Ok, "All subsystems initialized.");
    m_initialized = true;
    return true;
}

// ============================================================================
// T2.1: Platform + Render + Audio initialization
// ============================================================================

bool Engine::initPlatformPhase() {
    int width  = m_config.width;
    int height = m_config.height;
    const char* title = m_config.title;
    const bool gpuMode = !m_config.headless || m_config.editorMode;

    if (!m_platformBackend || !m_renderDevice || !m_audioBackend) {
        DEBUG_ERROR(SubSys::Engine, ErrCode::Engine_PlatformInitFailed,
                    "Platform, render, and audio backends are required.");
        return false;
    }
    if (!m_platformBackend->init(title, width, height)) {
        DEBUG_ERROR(SubSys::Engine, ErrCode::Engine_PlatformInitFailed,
                    "Platform backend init failed.");
        return false;
    }
    m_platformInitialized = true;
    BackendRegistry::instance().setPlatformBackend(m_platformBackend.get());

    // Mobile adapter: touch/lifecycle mapping (registered for editor/RPC
    // and future mobile ports; wired to SDL background/foreground events).
    m_mobileAdapter = std::make_unique<MobileAdapter>();
    BackendRegistry::instance().setMobileAdapter(m_mobileAdapter.get());
    // SDL app-lifecycle events are delivered only via event watches
    // (they are never queued); register once here.
    SDL_AddEventWatch(&Engine::appLifecycleWatch, this);

    if (m_config.editorMode) {
        SDL_HideWindow(static_cast<SDL_Window*>(
            m_platformBackend->getNativeWindowHandle()));
    }

    void* nwh = gpuMode ? m_platformBackend->getNativeWindowHandle() : nullptr;
    DEBUG_INFO(SubSys::Engine, ErrCode::Ok, "Native window handle: %p", nwh);

    if (!m_renderDevice->init(nwh, width, height)) {
        DEBUG_ERROR(SubSys::Engine, ErrCode::Engine_RenderInitFailed,
                    "Render device init failed.");
        return false;
    }
    m_renderInitialized = true;
    BackendRegistry::instance().setRenderDevice(m_renderDevice.get());

    // Render info (GPU caps)
    if (gpuMode) {
        const RenderRuntimeInfo renderInfo = m_renderDevice->getRuntimeInfo();
        DebugManager::RenderInfo ri;
        ri.backendName = renderInfo.backendName;
        ri.width = renderInfo.width;
        ri.height = renderInfo.height;
        ri.viewCount = renderInfo.viewCount;
        ri.shaderReady = renderInfo.shaderReady;
        DebugManager::instance().setRenderInfo(ri);
    }

    if (!m_audioBackend->init()) {
        DEBUG_ERROR(SubSys::Engine, ErrCode::Engine_AudioInitFailed,
                    "Audio backend init failed.");
        return false;
    }
    m_audioInitialized = true;
    BackendRegistry::instance().setAudioBackend(m_audioBackend.get());

    DebugManager::AudioInfo ai;
    ai.initialized = true; ai.bgmBusReady = true;
    ai.voiceBusReady = true; ai.seBusReady = true; ai.globalVolume = 1.0f;
    DebugManager::instance().setAudioInfo(ai);

    // Input router
    BackendRegistry::instance().setInputRouter(m_inputRouter.get());
    DebugManager::InputInfo ii;
    ii.currentFocus = "KAG";
    DebugManager::instance().setInputInfo(ii);

    if (!m_gpuMonitor) {
        m_gpuMonitor = createGpuMonitor(m_config.headless);
    }

    // Texture budget + shared backend registrations
    m_textureBudget->detect();
    BackendRegistry::instance().setTextureBudget(m_textureBudget.get());
    if (!m_textureManager->initialize(gpuMode)) {
        DEBUG_ERROR(SubSys::Engine, ErrCode::Engine_RenderInitFailed,
                    "Texture manager init failed.");
        return false;
    }
    m_textureManagerInitialized = true;
    BackendRegistry::instance().setTextureManager(m_textureManager.get());
    BackendRegistry::instance().setDebugManager(&DebugManager::instance());
    BackendRegistry::instance().setAsyncLoader(m_asyncLoader.get());
    m_saveManager->init("saves/");
    BackendRegistry::instance().setSaveManager(m_saveManager.get());
    if (!m_particleSystem) {
        m_particleSystem = createParticleSystem();
    }
    BackendRegistry::instance().setParticleSystem(m_particleSystem.get());

    BackendRegistry::instance().setLayerManager(m_layerManager.get());
    m_layerManager->init();
    m_layerManagerInitialized = true;

    return true;
}

// ============================================================================
// T2.2: Lua scripting + bindings + HotReload
// ============================================================================

bool Engine::initScriptingPhase() {
    if (!m_lua->init()) {
        fprintf(stderr, "[Engine] Lua VM init failed.\n");
        return false;
    }
    m_luaInitialized = true;

    // Phase G8-U1: install lua_Alloc hook for memory monitoring
    lua_setallocf(m_lua->state(), s_luaAllocFn, m_lua->state());
    BackendRegistry::instance().setLuaState(m_lua->state());
    BackendRegistry::instance().setLuaManager(m_lua.get());
    m_sandboxQuota->setLuaState(m_lua->state());
    m_sandboxQuotaBound = true;
    BackendRegistry::instance().setSandboxQuota(m_sandboxQuota.get());
    BackendRegistry::instance().setVideoPlayer(m_videoPlayer.get());

    // Phase R2-U1: Push all backend pointers into Lua registry so binding
    // files (KAG, Render, DevCore, Unified, VFX) can resolve them without
    // including BackendRegistry.h. Keys are the runtime contract with bindings.
    registerEngineLuaRegistryServices(m_lua->state(), m_inputRouter.get(),
                                      m_videoPlayer.get(), m_particleSystem.get(),
                                      m_textureManager.get(), m_asyncLoader.get());

    // HotReload for scripts/ directory
    m_hotReload->init("scripts/", m_lua->state());

    if (m_config.enableDebugger) {
        m_debugProtocol = std::make_unique<DebugProtocol>(*m_hotReload);
        if (!m_debugProtocol->init(m_lua->state())) {
            fprintf(stderr, "[Engine] DebugProtocol init failed.\n");
            m_debugProtocol.reset();
            return false;
        }
        m_debugProtocolInitialized = true;
    }
    installDebugPauseProbe(m_lua->state(), m_debugProtocol.get());
    lua_pushboolean(m_lua->state(), 0);
    lua_setglobal(m_lua->state(), "_CAESURA_VOICE_COMPLETE");

    return true;
}

// ============================================================================
// T2.3: Job system + Asset pipeline
// ============================================================================

bool Engine::initAssetPhase() {
    if (!m_resourceGenerationTracker) {
        m_resourceGenerationTracker = createResourceGenerationTracker();
    }
    BackendRegistry::instance().setResourceGenerationTracker(
        m_resourceGenerationTracker.get());

    // Parallel task system
    m_jobSystem->init();
    m_jobSystemInitialized = true;
    BackendRegistry::instance().setJobSystem(m_jobSystem.get());
    m_videoPlayer->setJobSystem(*m_jobSystem);

    // Asset management
    m_assetManager->init();
    m_assetManagerInitialized = true;
    // Inject CARC providers (moved from AssetManager to break resource→archive cycle)
    registerDefaultAssetProviders(*m_assetManager);
    m_asyncLoader->init();
    m_asyncLoaderInitialized = true;

    return true;
}

// ============================================================================
// T2.4: Optional subsystems (Steam, Crypto, MiniGame, Animation)
// ============================================================================

bool Engine::initOptionalPhase() {
    bool allOk = true;

    // Steam init (optional, no-op if SDK not present)
    m_steamInitialized = m_steamBackend && m_steamBackend->init();
    BackendRegistry::instance().setSteamBackend(
        m_steamInitialized ? m_steamBackend.get() : nullptr);
    if (m_steamInitialized) {
        registerSteamBinding(m_lua->state());
    }

    // Crypto engine registration (moved OUT of Steam if-block - bug fix)
    BackendRegistry::instance().setCryptoEngine(m_cryptoEngine.get());

    if (!m_miniGameBackend) {
        m_miniGameBackend = createFallbackMiniGameBackend();
    }

    // 3D mini-game backend (bgfx or null)
    m_miniGameBackend->setRenderDevice(BackendRegistry::instance().getRenderDevice());
    m_miniGameInitialized = m_miniGameBackend->init();
    if (!m_miniGameInitialized) {
        m_miniGameBackend->shutdown();
        m_miniGameBackend = createFallbackMiniGameBackend();
        m_miniGameBackend->setRenderDevice(BackendRegistry::instance().getRenderDevice());
        m_miniGameInitialized = m_miniGameBackend->init();
        allOk = false;
    }
    if (m_miniGameInitialized) {
        BackendRegistry::instance().setMiniGameBackend(m_miniGameBackend.get());
        registerMiniGameLuaRegistryService(m_lua->state(), m_miniGameBackend.get());
    }

    // Animation backend
    if (!m_animationBackend) m_animationBackend = createDefaultAnimationBackend();
    m_animationInitialized = m_animationBackend->init();
    if (!m_animationInitialized) {
        m_animationBackend->shutdown();
        m_animationBackend = createFallbackAnimationBackend();
        m_animationInitialized = m_animationBackend->init();
        allOk = false;
    }
    if (m_animationInitialized) {
        BackendRegistry::instance().setAnimationBackend(m_animationBackend.get());
        attachRenderDeviceToAnimationBackend(m_animationBackend.get(), m_renderDevice.get());
    }

    return allOk;
}
void Engine::run(const OwnerPump& ownerPump) {
    if (!m_initialized) {
        fprintf(stderr, "[Engine] run() requires a successful init().\n");
        return;
    }
    m_running = true;
    if (m_config.headless) {
        m_lastTick = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    } else {
        m_lastTick = m_platformBackend->getTicksMs();
    }
    DEBUG_INFO(SubSys::Engine, ErrCode::Ok, "Entering main loop.");

    while (m_running) {
        PROFILE_SCOPE("frame");
        m_skipLuaCallbacksThisFrame = false;
        if (ownerPump) ownerPump();
        m_skipLuaCallbacksThisFrame = pumpDebugger();
        publishDebugPauseState();
        processEvents();

        // --- GPU device loss recovery check (before any per-frame rendering) ---
        if (m_renderDevice && m_renderDevice->consumeDeviceLost()) {
            recoverFromDeviceLoss();
            continue;
        }

        uint64_t now;
        if (m_config.headless) {
            now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        } else {
            now = m_platformBackend->getTicksMs();
        }
        float dt = (float)(now - m_lastTick) / 1000.0f;
        m_lastTick = now;
        if (dt < 0.0f) dt = 0.0f;
        if (dt > 0.25f) dt = 0.25f;
        (void)dt; // reserved for frame-time tracking

        // Keep reload and Lua GC stopped while a coroutine is suspended.
        // Rendering and transport pumping remain active.
        if (!isLuaExecutionPaused()) {
            m_hotReload->checkAndReload();
        }

        // JobSystem main-thread callbacks + async load SDL events
        m_jobSystem->pollMainThreadJobs();
        if (!(m_config.headless || m_config.editorMode)) {
            m_asyncLoader->poll();
        }

        // -- Phase G8-U1: Lua memory budget check \(every frame\) ---------------
        lua_State* Lgc = m_lua->state();
        if (Lgc && !isLuaExecutionPaused()) {
            int memKB = lua_gc(Lgc, LUA_GCCOUNT, 0);
            if (memKB > 204 * 1024) {  // 80% = 204MB
                lua_gc(Lgc, LUA_GCSTEP, 50);
            }
            if (memKB > 244 * 1024) {  // 95% = 244MB
                lua_gc(Lgc, LUA_GCCOLLECT, 0);
                lua_getglobal(Lgc, "System");
                if (lua_istable(Lgc, -1)) {
                    lua_getfield(Lgc, -1, "collect_full");
                    if (lua_isfunction(Lgc, -1)) {
                        if (lua_pcall(Lgc, 0, 0, 0) != LUA_OK) {
                            fprintf(stderr, "System.collect_full: %s\n",
                                    lua_tostring(Lgc, -1) ? lua_tostring(Lgc, -1) : "unknown");
                            lua_pop(Lgc, 1);
                        }
                    }
                    else lua_pop(Lgc, 1);
                }
                lua_pop(Lgc, 1);
            }
            if (memKB > 256 * 1024) {  // 100% = 256MB
                fprintf(stderr, "[Engine] FATAL: Lua memory exceeded 256MB\n");
                handleFatalError("Lua OOM", "Memory exceeded 256MB limit");
            }
            // 300-frame periodic GC step
            m_gcFrameCounter++;
            if (m_gcFrameCounter >= 300) {
                m_gcFrameCounter = 0;
                lua_gc(Lgc, LUA_GCSTEP, 10);
            }
        }

        GpuQuality gpuQ = m_gpuMonitor->update(static_cast<double>(dt));

        lua_State* L = m_lua->state();
        if (L) {
            // Dispatch at most one coalesced click per frame, before the
            // Lua update (mirrors the pre-existing _KAG_onClick behavior).
            if (m_clickPending && !isLuaExecutionPaused()) {
                m_clickPending = false;
                lua_getglobal(L, "_KAG_onClick");
                if (lua_isfunction(L, -1)) {
                    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                        const char* err = lua_tostring(L, -1);
                        fprintf(stderr, "_KAG_onClick: %s\n", err ? err : "unknown");
                        lua_pop(L, 1);
                    }
                } else { lua_pop(L, 1); }
            }
            if (!isLuaExecutionPaused()) {
                lua_getglobal(L, "engine_update");
                if (lua_isfunction(L, -1)) {
                    lua_pushnumber(L, dt);
                    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                        const char* err = lua_tostring(L, -1);
                        fprintf(stderr, "engine_update: %s\n", err ? err : "unknown");
                        handleFatalError("engine_update", err);
                        lua_pop(L, 1);
                    }
                } else { lua_pop(L, 1); }
            }
            publishDebugPauseState();

            const int gpuQv = static_cast<int>(gpuQ);
            if (gpuQv != m_lastGpuQuality) {
                m_lastGpuQuality = gpuQv;
                lua_pushinteger(L, gpuQv);
                lua_setglobal(L, "_CAESURA_GPU_QUALITY");
            }
            const bool vfxOn = m_gpuMonitor->vfxEnabled();
            if (vfxOn != m_lastVfxEnabled) {
                m_lastVfxEnabled = vfxOn;
                lua_pushboolean(L, vfxOn ? 1 : 0);
                lua_setglobal(L, "_CAESURA_VFX_ENABLED");
            }
            lua_pushnumber(L, static_cast<lua_Number>(m_gpuMonitor->metrics().gpuTimeMs));
            lua_setglobal(L, "_CAESURA_GPU_TIME_MS");
            lua_pushnumber(L, static_cast<lua_Number>(m_gpuMonitor->metrics().rollingAvgMs));
            lua_setglobal(L, "_CAESURA_GPU_AVG_MS");
            const bool degraded = m_gpuMonitor->metrics().degraded;
            if (degraded != m_lastGpuDegraded) {
                m_lastGpuDegraded = degraded;
                lua_pushboolean(L, degraded ? 1 : 0);
                lua_setglobal(L, "_CAESURA_GPU_DEGRADED");
            }
        }

        // D4.6: Consume backend-owned voice completion events. A counter keeps
        // multiple natural completions lossless while Lua is debugger-paused.
        if (m_audioBackend) {
            m_audioBackend->update(static_cast<float>(dt));
            const unsigned int completed =
                m_audioBackend->consumeVoiceCompletions();
            const unsigned int capacity =
                std::numeric_limits<unsigned int>::max() -
                m_audioVoiceCompletionsPending;
            m_audioVoiceCompletionsPending +=
                completed > capacity ? capacity : completed;

            if (m_audioBackend->isVoicePlaying() && L) {
                lua_pushboolean(L, 0);
                lua_setglobal(L, "_CAESURA_VOICE_COMPLETE");
            }

            while (m_audioVoiceCompletionsPending > 0 &&
                   !isLuaExecutionPaused() && L) {
                lua_pushboolean(L, 1);
                lua_setglobal(L, "_CAESURA_VOICE_COMPLETE");
                lua_getglobal(L, "_onVoiceComplete");
                if (lua_isfunction(L, -1)) {
                    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                        const char* error = lua_tostring(L, -1);
                        fprintf(stderr, "_onVoiceComplete: %s\n",
                                error ? error : "non-string Lua error");
                        lua_pop(L, 1);
                    }
                } else { lua_pop(L, 1); }
                --m_audioVoiceCompletionsPending;
            }

            // A completion callback may start the next line immediately.
            if (m_audioBackend->isVoicePlaying() && L) {
                lua_pushboolean(L, 0);
                lua_setglobal(L, "_CAESURA_VOICE_COMPLETE");
            }
        }

        render(static_cast<float>(dt));
        if (m_renderDevice) m_renderDevice->advanceFrame();

        // -- Reserved: 3D mini-game update hook (CPU work, future JobSystem target) --
        if (m_miniGameBackend && m_miniGameBackend->isActive() &&
            !isLuaExecutionPaused()) {
            m_miniGameBackend->update(static_cast<float>(dt));
            // U3.3: invoke Lua-side per-frame input callback if defined
            if (L) {
                lua_getglobal(L, "_minigame_update");
                if (lua_isfunction(L, -1)) {
                    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                        fprintf(stderr, "_minigame_update: %s\n",
                                lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown");
                        lua_pop(L, 1);
                    }
                } else {
                    lua_pop(L, 1);
                }
            }
        }
    }

}

bool Engine::pumpDebugger() {
    if (!m_debugProtocolInitialized || !m_debugProtocol) return false;

    m_debugProtocol->pumpCommands();
    const DebugProtocol::ResumeOutcome outcome =
        m_debugProtocol->resumePausedCoroutineManaged();
    if (outcome.status == DebugProtocol::NoResume) return false;

    if (!outcome.error.empty()) {
        fprintf(stderr, "[Debug] Coroutine resume failed: %s\n",
                outcome.error.c_str());
    }
    notifyKagDebugResume();
    return true;
}

bool Engine::isLuaExecutionPaused() const {
    return m_deviceRecoveryPaused || m_skipLuaCallbacksThisFrame ||
           (m_debugProtocol && m_debugProtocol->isDebugActive());
}

void Engine::publishDebugPauseState() {
    if (!m_luaInitialized || !m_lua || !m_lua->state()) return;
    lua_pushboolean(m_lua->state(), isLuaExecutionPaused() ? 1 : 0);
    lua_setglobal(m_lua->state(), kDebugPausedGlobal);
}

void Engine::notifyKagDebugResume() {
    lua_State* L = m_lua ? m_lua->state() : nullptr;
    if (!L) return;

    const int stackTop = lua_gettop(L);
    lua_getglobal(L, "package");
    if (!lua_istable(L, -1)) {
        lua_settop(L, stackTop);
        return;
    }
    lua_getfield(L, -1, "loaded");
    if (!lua_istable(L, -1)) {
        lua_settop(L, stackTop);
        return;
    }
    lua_getfield(L, -1, "kag_runner");
    if (!lua_istable(L, -1)) {
        lua_settop(L, stackTop);
        return;
    }
    lua_getfield(L, -1, "debug_resume");
    if (lua_isfunction(L, -1) && lua_pcall(L, 0, 0, 0) != LUA_OK) {
        fprintf(stderr, "[Debug] kag_runner.debug_resume: %s\n",
                lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown");
    }
    lua_settop(L, stackTop);
}

void Engine::quit() {
    m_running = false;
}


void Engine::dispatchAsyncLoad(CompletedLoad* completed) {
    lua_State* L = m_lua ? m_lua->state() : nullptr;
    if (!L) {
        delete completed;
        return;
    }

    uint32_t texId = 0;
    if (completed->success && completed->type == "texture" &&
        !completed->rgba.empty() && completed->width > 0) {
        CAESURA_ASSERT_MAIN_THREAD();
        texId = m_textureManager->loadTextureFromRGBA(
            completed->rgba.data(), completed->width, completed->height,
            completed->path);
        if (texId == 0) completed->success = false;
    }

    lua_getglobal(L, "_ASYNC_CALLBACKS");
    if (lua_istable(L, -1)) {
        lua_pushinteger(L, completed->id);
        lua_gettable(L, -2);
        int cbRef = (int)lua_tointeger(L, -1);
        lua_pop(L, 2);
        if (cbRef > 0) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, cbRef);
            if (lua_isfunction(L, -1)) {
                lua_pushboolean(L, completed->success ? 1 : 0);
                lua_pushstring(L, completed->path.c_str());
                lua_pushinteger(L, static_cast<lua_Integer>(texId));
                if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
                    fprintf(stderr, "[AsyncLoader] callback error: %s\n",
                            lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown");
                    lua_pop(L, 1);
                }
                printf("[AsyncLoader] Callback #%d: %s (%s)\n",
                       completed->id, completed->path.c_str(),
                       completed->success ? "ok" : "fail");
            } else { lua_pop(L, 1); }
            luaL_unref(L, LUA_REGISTRYINDEX, cbRef);
            lua_getglobal(L, "_ASYNC_CALLBACKS");
            if (lua_istable(L, -1)) {
                lua_pushinteger(L, completed->id);
                lua_pushnil(L);
                lua_settable(L, -3);
            }
            lua_pop(L, 1);
        }
    } else { lua_pop(L, 1); }
    delete completed;
}

void Engine::processEvents() {
    // Steam callbacks every frame
    m_steamBackend->runCallbacks();

    lua_State* L = m_lua->state();
    if (L) {
        lua_getglobal(L, "_CAESURA_QUIT");
        const bool quitRequested =
            lua_isboolean(L, -1) && lua_toboolean(L, -1);
        lua_pop(L, 1);
        if (quitRequested) {
            m_running = false;
            return;
        }
    }

    // Pause input while Steam overlay is active
    if (m_steamBackend->isOverlayActive()) return;
    // Track 3: Reset Lua instruction budget each frame
    m_lua->resetInstructionBudget();

    // Headless/Editor mode: no SDL event loop -- deliver completed async
    // loads directly (texture upload + Lua callback) instead of queueing
    // SDL events that nothing would ever consume.
    if (m_config.headless || m_config.editorMode) {
        for (auto& c : m_asyncLoader->drainCompleted()) {
            dispatchAsyncLoad(new CompletedLoad(std::move(c)));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        return;
    }
    // Deferred loads (Lua paused): re-dispatch when unpaused. This must run
    // for both modes, so it stays after the headless early return only for
    // GPU mode -- headless/editor never pause-loads (no SDL event source).
    if (!isLuaExecutionPaused() && !m_deferredAsyncLoads.empty()) {
        for (auto& completed : m_deferredAsyncLoads) {
            SDL_Event deferred;
            SDL_zero(deferred);
            deferred.type = CAESURA_EVENT_ASYNC_LOAD;
            auto* rawCompletion = completed.release();
            deferred.user.data1 = rawCompletion;
            if (!SDL_PushEvent(&deferred)) delete rawCompletion;
        }
        m_deferredAsyncLoads.clear();
    }
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // -- GPU device reset from SDL (triggers recovery at next loop iteration) --
        if (event.type == SDL_EVENT_RENDER_DEVICE_RESET) {
            if (m_renderDevice) m_renderDevice->flagDeviceLost();
        }

        // -- G8-U3: Async load completion (custom SDL event from AsyncLoader) --
        if (event.type == CAESURA_EVENT_ASYNC_LOAD) {
            auto* completed = static_cast<CompletedLoad*>(event.user.data1);
            if (completed && isLuaExecutionPaused()) {
                m_deferredAsyncLoads.emplace_back(completed);
                continue;
            }
            if (completed) {
                dispatchAsyncLoad(completed);
                continue;
            }
        }
        // (original dispatch body moved to Engine::dispatchAsyncLoad)
        // (dispatch body moved to Engine::dispatchAsyncLoad)

        if ((event.type == SDL_EVENT_MOUSE_MOTION ||
             event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
             event.type == SDL_EVENT_MOUSE_BUTTON_UP) && L) {
            float mx = 0, my = 0;
            SDL_GetMouseState(&mx, &my);
            IPlatformBackend::MouseState mouse;
            mouse.x = mx; mouse.y = my;
            mouse.leftDown = (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0;
            lua_pushnumber(L, mouse.x); lua_setglobal(L, "_GAME_MOUSE_X");
            lua_pushnumber(L, mouse.y); lua_setglobal(L, "_GAME_MOUSE_Y");
            lua_pushboolean(L, mouse.leftDown ? 1 : 0);
            lua_setglobal(L, "_GAME_MOUSE_DOWN");


            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                m_inputRouter->getFocus() == InputFocus::KAG &&
                !isLuaExecutionPaused()) {
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    // Right-click dispatches immediately (not the advance path).
                    lua_getglobal(L, "_KAG_onRightClick");
                    if (lua_isfunction(L, -1)) {
                        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                            const char* err = lua_tostring(L, -1);
                            fprintf(stderr, "_KAG_onRightClick: %s\n", err ? err : "unknown");
                            lua_pop(L, 1);
                        }
                    } else { lua_pop(L, 1); }
                } else {
                    // Coalesce left clicks: the frame loop dispatches at
                    // most one _KAG_onClick per frame (event storms must
                    // not batch-resume the scheduler or deep-copy rollback
                    // snapshots per event).
                    m_clickPending = true;
                }
            }

        }

        // D9.7: Mouse wheel is a distinct SDL event, not a mouse-button event.
        if (event.type == SDL_EVENT_MOUSE_WHEEL && L &&
            m_inputRouter->getFocus() == InputFocus::KAG &&
            !isLuaExecutionPaused()) {
            lua_pushnumber(L, event.wheel.y); lua_setglobal(L, "_KAG_MOUSE_WHEEL_Y");
            lua_getglobal(L, "_KAG_onMouseWheel");
            if (lua_isfunction(L, -1)) {
                if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                    const char* err = lua_tostring(L, -1);
                    fprintf(stderr, "_KAG_onMouseWheel: %s\n", err ? err : "unknown");
                    lua_pop(L, 1);
                }
            } else { lua_pop(L, 1); }
        }

        if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && L) {
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_F5) {
                    lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_F5");
                    if (!event.key.repeat && !isLuaExecutionPaused()) quicksave();
                }
                if (event.key.key == SDLK_F6) {
                    lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_F6");
                    if (!event.key.repeat && !isLuaExecutionPaused()) quickload();
                }
                if (event.key.key == SDLK_W)    { lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_W"); }
                if (event.key.key == SDLK_A)    { lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_A"); }
                if (event.key.key == SDLK_S)    { lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_S"); }
                if (event.key.key == SDLK_D)    { lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_D"); }
                if (event.key.key == SDLK_UP)   { lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_UP"); }
                if (event.key.key == SDLK_DOWN) { lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_DOWN"); }
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
                    lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_ENTER");
                }
                if (event.key.key == SDLK_ESCAPE) { lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_ESC"); }
                if (event.key.key == SDLK_H)    { lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_H"); }
                if (event.key.key == SDLK_V && !event.key.repeat) {
                    lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_V");
                }
                // D9.6: Ctrl triggers skip mode via Lua
                if ((event.key.key == SDLK_LCTRL || event.key.key == SDLK_RCTRL) &&
                    !isLuaExecutionPaused()) {
                    lua_getglobal(L, "_KAG_onCtrlDown");
                    if (lua_isfunction(L, -1)) {
                        if (lua_pcall(L, 0, 0, 0) != LUA_OK) { lua_pop(L, 1); }
                    } else { lua_pop(L, 1); }
                }
            }
            if (event.type == SDL_EVENT_KEY_UP) {
                if (event.key.key == SDLK_F5) { lua_pushboolean(L, 0); lua_setglobal(L, "_GAME_KEY_F5"); }
                if (event.key.key == SDLK_F6) { lua_pushboolean(L, 0); lua_setglobal(L, "_GAME_KEY_F6"); }
                if (event.key.key == SDLK_W)    { lua_pushboolean(L, 0); lua_setglobal(L, "_GAME_KEY_W"); }
                if (event.key.key == SDLK_A)    { lua_pushboolean(L, 0); lua_setglobal(L, "_GAME_KEY_A"); }
                if (event.key.key == SDLK_S)    { lua_pushboolean(L, 0); lua_setglobal(L, "_GAME_KEY_S"); }
                if (event.key.key == SDLK_D)    { lua_pushboolean(L, 0); lua_setglobal(L, "_GAME_KEY_D"); }
                if (event.key.key == SDLK_UP)   { lua_pushboolean(L, 0); lua_setglobal(L, "_GAME_KEY_UP"); }
                if (event.key.key == SDLK_DOWN) { lua_pushboolean(L, 0); lua_setglobal(L, "_GAME_KEY_DOWN"); }
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
                    lua_pushboolean(L, 0); lua_setglobal(L, "_GAME_KEY_ENTER");
                }
                if (event.key.key == SDLK_ESCAPE) { lua_pushboolean(L, 0); lua_setglobal(L, "_GAME_KEY_ESC"); }
                if (event.key.key == SDLK_H)    { lua_pushboolean(L, 0); lua_setglobal(L, "_GAME_KEY_H"); }
                if (event.key.key == SDLK_V)    { lua_pushboolean(L, 0); lua_setglobal(L, "_GAME_KEY_V"); }
                // D9.6: Ctrl skip mode toggle release
                if ((event.key.key == SDLK_LCTRL || event.key.key == SDLK_RCTRL) &&
                    !isLuaExecutionPaused()) {
                    lua_getglobal(L, "_KAG_onCtrlUp");
                    if (lua_isfunction(L, -1)) {
                        if (lua_pcall(L, 0, 0, 0) != LUA_OK) { lua_pop(L, 1); }
                    } else { lua_pop(L, 1); }
                }
            }
        }
        m_inputRouter->processEvent(event);
    }
}

void Engine::render(float dt) {
    // Headless mode (not editor): no GPU rendering
    if (m_config.headless && !m_config.editorMode) return;

    m_lua->resetInstructionBudget();
    // Drive active videos by real frame time (frame-rate pacing inside).
    if (m_videoPlayer) m_videoPlayer->updateAll(dt);
    lua_State* L = m_lua->state();
    if (L && !isLuaExecutionPaused()) {
        lua_getglobal(L, "engine_render");
        if (lua_isfunction(L, -1)) {
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                fprintf(stderr, "engine_render: %s\n", err ? err : "unknown");
                lua_pop(L, 1);
            }
        } else { lua_pop(L, 1); }
    }

    if (m_animationInitialized && m_animationBackend) {
        DebugManager::instance().beginFrameProfile();
        m_animationBackend->render(isLuaExecutionPaused() ? 0.0f : dt);
        DebugManager::instance().endFrameProfile();
    }

    if (m_renderDevice) {
        m_renderDevice->drawDebugOverlay("Caesura (AmeKAG) v1.0.0");
    }

    // -- Reserved: 3D mini-game render hook (main thread, after KAG pass) --
    if (m_miniGameBackend && m_miniGameBackend->isActive()) {
        m_miniGameBackend->render();
    }

}


// ============================================================================
//  Save helpers (SU-3)
// ============================================================================
void Engine::quicksave() {
    if (isLuaExecutionPaused()) return;
    lua_State* L = m_lua->state(); if (!L) return;
    lua_getglobal(L, "quicksave");
    if (lua_isfunction(L, -1)) { if (lua_pcall(L, 0, 0, 0) != LUA_OK) { fprintf(stderr, "quicksave: %s\n", lua_tostring(L, -1)); lua_pop(L, 1); } }
    else { lua_pop(L, 1); }
}
void Engine::quickload() {
    if (isLuaExecutionPaused()) return;
    lua_State* L = m_lua->state(); if (!L) return;
    lua_getglobal(L, "quickload");
    if (lua_isfunction(L, -1)) { if (lua_pcall(L, 0, 0, 0) != LUA_OK) { fprintf(stderr, "quickload: %s\n", lua_tostring(L, -1)); lua_pop(L, 1); } }
    else { lua_pop(L, 1); }
}
void Engine::triggerAutoSave() {
    if (isLuaExecutionPaused()) return;
    lua_State* L = m_lua->state(); if (!L) return;
    lua_getglobal(L, "autosave");
    if (lua_isfunction(L, -1)) { if (lua_pcall(L, 0, 0, 0) != LUA_OK) { fprintf(stderr, "autosave: %s\n", lua_tostring(L, -1)); lua_pop(L, 1); } }
    else { lua_pop(L, 1); }
}

void Engine::handleFatalError(const char* context, const char* luaError) {
    std::string msg = "A fatal error occurred in the engine.\n\n";
    msg += "Context: ";
    msg += context ? context : "unknown";
    msg += "\n\n";
    msg += luaError ? luaError : "No error details available.";
    msg += "\n\nPlease choose an action below.";

    ErrorAction action = ErrorUI::show(
        "Engine Runtime Error",
        msg,
        "", 0,
        m_renderDevice != nullptr
    );

    switch (action) {
        case ErrorAction::Retry:
            DEBUG_INFO(SubSys::Engine, ErrCode::Ok, "ErrorUI: Retry requested");
            // Request hot reload retry
            m_hotReload->requestReload();
            break;
        case ErrorAction::Title:
            DEBUG_INFO(SubSys::Engine, ErrCode::Ok, "ErrorUI: Title requested");
            m_running = false;
            break;
        case ErrorAction::Quit:
        default:
            DEBUG_INFO(SubSys::Engine, ErrCode::Ok, "ErrorUI: Quit requested");
            m_running = false;
            break;
    }
}


void Engine::renderOneFrame() {
    if (!m_initialized || !m_renderInitialized) return;
    if (m_config.headless && !m_config.editorMode) return;
    lua_State* L = m_lua->state();
    if (L && !isLuaExecutionPaused()) {
        lua_getglobal(L, "engine_update");
        if (lua_isfunction(L, -1)) {
            lua_pushnumber(L, 0.016);
            if (lua_pcall(L, 1, 0, 0) != LUA_OK) { lua_pop(L, 1); }
        } else { lua_pop(L, 1); }
    }
    render(0.016f);
    if (m_renderDevice) m_renderDevice->advanceFrame();
}

bool Engine::reloadScriptsNow() {
    requireInitialized();
    if (isLuaExecutionPaused()) return false;
    m_hotReload->requestReload();
    return m_hotReload->checkAndReload();
}

static std::string captureFrameBase64(IRenderDevice& renderer, int w, int h) {
    static int counter = 0;
    char path[256];
    snprintf(path, sizeof(path), "editor_frame_%d.png", counter++);
    if (counter > 99) counter = 0;
    if (!renderer.requestScreenshot(path)) return "";
    renderer.advanceFrame();
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return "";
    std::streamsize size = file.tellg();
    if (size <= 0) return "";
    const auto fileSize = static_cast<size_t>(size);
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();
    std::remove(path);
    static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((fileSize + 2) / 3) * 4);
    for (size_t i = 0; i < fileSize; i += 3) {
        unsigned char a = buffer[i];
        unsigned char b = (i + 1 < fileSize) ? buffer[i + 1] : 0;
        unsigned char c = (i + 2 < fileSize) ? buffer[i + 2] : 0;
        result += b64[a >> 2];
        result += b64[((a & 3) << 4) | (b >> 4)];
        result += (i + 1 < fileSize) ? b64[((b & 15) << 2) | (c >> 6)] : '=';
        result += (i + 2 < fileSize) ? b64[c & 63] : '=';
    }
    return result;
}

std::string Engine::captureFrameForRpc(int w, int h) {
    if (!m_initialized || !m_renderInitialized) return "";
    // Render one frame first; screenshot capture advances the renderer frame.
    if (!m_config.headless || m_config.editorMode) {
        lua_State* L = m_lua->state();
        if (L && !isLuaExecutionPaused()) {
            lua_getglobal(L, "engine_update");
            if (lua_isfunction(L, -1)) {
                lua_pushnumber(L, 0.016);
                if (lua_pcall(L, 1, 0, 0) != LUA_OK) { lua_pop(L, 1); }
            } else { lua_pop(L, 1); }
        }
        render(0.016f);
    }
    if (!m_renderDevice) return "";
    return captureFrameBase64(*m_renderDevice, w, h);
}

// -- App lifecycle watcher -------------------------------------------------
// SDL delivers WILL_ENTER_BACKGROUND/DID_ENTER_FOREGROUND exclusively to
// event watches (they never enter the poll queue). Contract: the platform
// layer must deliver these on the engine/main thread (true on iOS/Android);
// when a native mobile layer lands on a platform that dispatches elsewhere,
// events must be marshalled to the engine thread before touching Lua.
//
// Pure decision logic lives in handleAppLifecycle (unit-testable without
// SDL); the watcher only extracts Engine state and forwards the event type.
//
// Re-entrancy note (handoff 008 §4.1): SDL3 invokes event watchers from the
// push path too. A user Lua onPause/onResume callback that itself pushes an
// SDL event would re-enter this watcher synchronously inside the event-queue
// lock -> theoretical self-deadlock (LOW, handoff 004 (d)). The engine
// currently exposes no Lua binding that pushes SDL events, so the path is
// unreachable; re-audit when a mobile input/marshalling layer lands.
void Engine::handleAppLifecycle(IMobileAdapter* adapter, lua_State* L, Uint32 eventType) {
    if (!adapter) return;
    switch (eventType) {
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
            adapter->onPause(L);
            break;
        case SDL_EVENT_DID_ENTER_FOREGROUND:
            adapter->onResume(L);
            break;
        default:
            break;
    }
}

bool Engine::appLifecycleWatch(void* userdata, SDL_Event* event) {
    auto* engine = static_cast<Engine*>(userdata);
    if (!engine) return true;
    handleAppLifecycle(engine->m_mobileAdapter.get(),
                       engine->m_lua ? engine->m_lua->state() : nullptr,
                       event->type);
    return true; // allow other watchers / the queue to see the event
}

void Engine::shutdown() {
    if (m_shutdownComplete) return;
    m_shutdownComplete = true;
    m_initialized = false;

    // Unregister the SDL app-lifecycle watch first: a background event
    // delivered after this point must not reach Lua state being torn down.
    SDL_RemoveEventWatch(&Engine::appLifecycleWatch, this);

    // A never-initialized Engine owns its injected objects but no global state.
    if (!m_initAttempted) return;

    auto& registry = BackendRegistry::instance();

    VFXBinding_Shutdown(m_particleSystem.get());

    if (m_renderInitialized) {
        m_renderDevice->beginShutdown();
    }

    if (m_layerManagerInitialized) {
        m_layerManager->shutdown();
        m_layerManagerInitialized = false;
        registry.setLayerManager(nullptr);
    }

    // Reset error crash counters on clean shutdown
    ErrorUI::resetCounters();

    if (m_miniGameInitialized) m_miniGameBackend->shutdown();
    m_miniGameBackend.reset();
    registry.setMiniGameBackend(nullptr);

    if (m_asyncLoaderInitialized) m_asyncLoader->shutdown();
    if (m_assetManagerInitialized) m_assetManager->shutdown();
    if (m_jobSystemInitialized) m_jobSystem->shutdown();
    if (m_steamInitialized) {
        m_steamBackend->shutdown();
    }
    registry.setSteamBackend(nullptr);
    if (m_videoPlayer) m_videoPlayer->shutdown();
    if (m_luaInitialized) {
        removeDebugPauseProbe(m_lua->state());
    }
    if (m_debugProtocolInitialized && m_debugProtocol) {
        if (!m_debugProtocol->shutdown()) {
            fprintf(stderr, "[Engine] DebugProtocol shutdown rejected off owner thread.\n");
        }
        m_debugProtocolInitialized = false;
    }
    m_debugProtocol.reset();
    if (m_luaInitialized) m_hotReload->shutdown();
    if (m_audioInitialized) m_audioBackend->shutdown();

    // Flush renderer pipeline before touching shared animation resources.
    if (m_renderInitialized) { m_renderDevice->flushAllRTT(); }

    // Process one final renderer frame to drain pending GPU commands.
    if (m_renderInitialized) { m_renderDevice->advanceFrame(); }

    // Animation shuts down after renderer pipeline is drained.
    if (m_animationInitialized) {
        m_animationBackend->shutdown();
        m_animationInitialized = false;
    }
    m_animationBackend.reset();
    registry.setAnimationBackend(nullptr);

    // Animation can release textures through ITextureManager during shutdown.
    if (m_textureManagerInitialized) {
        m_textureManager->shutdown();
        m_textureManagerInitialized = false;
        registry.setTextureManager(nullptr);
    }

    // Drain texture destruction before unregistering quota/Lua services.
    if (m_renderInitialized) { m_renderDevice->advanceFrame(); }

    if (m_sandboxQuotaBound) {
        registry.setSandboxQuota(nullptr);
        m_sandboxQuota->setLuaState(nullptr);
        m_sandboxQuotaBound = false;
    }
    if (m_luaInitialized) m_lua->shutdown();
    m_deferredAsyncLoads.clear();

    if (m_renderInitialized) { m_renderDevice->shutdown(); }
    if (m_platformInitialized) m_platformBackend->shutdown();

    registry.setVideoPlayer(nullptr);
    registry.setInputRouter(nullptr);
    registry.setTextureManager(nullptr);
    registry.setLayerManager(nullptr);
    registry.setSandboxQuota(nullptr);
    registry.setTextureBudget(nullptr);
    registry.setDebugManager(nullptr);
    registry.setAsyncLoader(nullptr);
    registry.setJobSystem(nullptr);
    registry.setCryptoEngine(nullptr);
    registry.setParticleSystem(nullptr);
    registry.setSaveManager(nullptr);
    registry.setResourceGenerationTracker(nullptr);
    registry.setSteamBackend(nullptr);
    registry.setMobileAdapter(nullptr);
    registry.setRenderDevice(nullptr);
    registry.setAudioBackend(nullptr);
    registry.setPlatformBackend(nullptr);
    registry.setLuaManager(nullptr);
    registry.setLuaState(nullptr);

    if (m_debugInitialized) DebugManager::instance().shutdown();
}

void Engine::recoverFromDeviceLoss() {
    fprintf(stderr, "[Engine] === GPU device lost — starting recovery ===\n");
    m_deviceRecoveryPaused = true;

    // Phase 1: Notify all listeners to release GPU resources
    BackendRegistry::instance().notifyDeviceLost();

    // Phase 2: Reinitialize renderer.
    void* nwh = nullptr;
    if (m_platformBackend) {
        nwh = m_platformBackend->getNativeWindowHandle();
    }
    int w = m_platformBackend ? m_platformBackend->getWindowWidth() : m_config.width;
    int h = m_platformBackend ? m_platformBackend->getWindowHeight() : m_config.height;

    if (!m_renderDevice || !m_renderDevice->recoverDevice(nwh, w, h)) {
        fprintf(stderr, "[Engine] FATAL: renderer recovery failed after device loss\n");
        handleFatalError("GPU Recovery", "Renderer recovery failed after device loss");
        return;
    }
    fprintf(stderr, "[Engine] renderer recovery success (%dx%d)\n", w, h);

    // Phase 3: Notify all listeners to recreate GPU resources.
    BackendRegistry::instance().notifyDeviceRestored();

    // Phase 4: Notify Lua scripts to reload textures.
    lua_State* L = m_lua->state();
    if (L) {
        lua_pushboolean(L, 1);
        lua_setglobal(L, "_CAESURA_DEVICE_RESTORED");
    }

    m_deviceRecoveryPaused = false;
    fprintf(stderr, "[Engine] === GPU device recovery complete ===\n");
}

} // namespace Caesura
