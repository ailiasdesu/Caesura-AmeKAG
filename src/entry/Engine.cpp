extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include "Engine.h"
#include "../input/InputRouter.h"
#include "../di/BackendRegistry.h"
#include "../debug/DebugManager.h"
#include "../rpc/RpcServer.h"
#include "ErrorUI.h"
#include "../platform/SDL3PlatformBackend.h"
#include "../audio/api/IAudioBackend.h"
#include "../di/TextureBudget.h"
#include "../render/api/IRenderDevice.h"
#include "../render/VideoPlayer.h"
#include "../render/api/IGpuMonitor.h"
#include "../render/FreeTypeContext.h"
#include "../audio/SoLoudAudioEngine.h"
#include "../script/bindings/RenderBinding.h"
#include "../render/BgfxRenderDevice.h"
#include "../render/BgfxDebugCallback.h"
#include "../script/vm/LuaManager.h"
#include "../storage/SaveManager.h"
#include "../debug/HotReload.h"
#include "../job/JobSystem.h"
#include "../resource/AsyncLoader.h"
#include "../resource/AssetManager.h"
#include "../render/TextureManager.h"
#include "../minigame/BgfxMiniGameBackend.h"
#include "../live2d/NullAnimationBackend.h"
#include "../steam/api/ISteamBackend.h"
#include "../steam/SteamBackend.h"
#include "../steam/NullSteamBackend.h"
#include "../script/bindings/SteamBinding.h"
#include "../archive/CryptoEngine.h"
#include "../archive/CARCReader.h"
#include "../archive/CarcAssetProvider.h"
#ifdef CAESURA_HAS_LIVE2D
#include "../live2d/Live2D/Live2DBackend.h"
#endif
#include <thread>
#include <atomic>
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <bx/bx.h>
#include <cstdio>
#include <fstream>
#include <vector>
#include <cmath>
#include <chrono>


namespace Caesura {
namespace detail { thread_local std::thread::id g_mainThreadId; }

// Factory for GpuMonitor (defined in Engine_Gpu.cpp 鈥?F1)
std::unique_ptr<IGpuMonitor> createGpuMonitor(bool headless);


// -- Phase G8-U1: lua_Alloc hook for per-allocation memory check ---------------
// Lua 5.4 forbids calling lua_gc inside the allocator (stack overflow).
// Memory budget is enforced per-frame in Engine::run() instead.
static void* s_luaAllocFn(void* ud, void* ptr, size_t osize, size_t nsize) {
    // Lua 5.4 forbids calling lua_gc inside the allocator (C stack overflow).
    if (nsize == 0) { free(ptr); return nullptr; }
    return realloc(ptr, nsize);
}

Engine::Engine(const EngineConfig& config)
    : m_config(config)
    , m_lua(config.lua ? std::unique_ptr<LuaManager>(config.lua) : std::make_unique<LuaManager>())
    , m_inputRouter(config.inputRouter ? std::unique_ptr<InputRouter>(config.inputRouter) : std::make_unique<InputRouter>())
    , m_videoPlayer(config.videoPlayer ? std::unique_ptr<VideoPlayer>(config.videoPlayer) : std::make_unique<VideoPlayer>())
#ifdef CAESURA_HAS_STEAM
    , m_steamBackend(config.steam ? std::unique_ptr<ISteamBackend>(config.steam) : std::make_unique<SteamBackend>())
#else
    , m_steamBackend(config.steam ? std::unique_ptr<ISteamBackend>(config.steam) : std::make_unique<NullSteamBackend>())
#endif
    
{}

Engine::~Engine() {
    if (!m_shutdownComplete) shutdown();
}

IRenderDevice& Engine::renderDevice() { return *BackendRegistry::instance().getRenderDevice(); }
IAudioBackend& Engine::audio() { return *BackendRegistry::instance().getAudioBackend(); }
IPlatformBackend& Engine::platform() { return *BackendRegistry::instance().getPlatformBackend(); }

bool Engine::init() {
    // Take ownership of DI-injected implementations
    if (m_config.platform)  m_platformBackend.reset(static_cast<IPlatformBackend*>(m_config.platform));
    if (m_config.render)    m_renderDevice.reset(static_cast<IRenderDevice*>(m_config.render));
    if (m_config.audio)     m_audioBackend.reset(static_cast<IAudioBackend*>(m_config.audio));
    if (m_config.miniGame)  m_miniGameBackend.reset(static_cast<IMiniGameBackend*>(m_config.miniGame));
    if (m_config.animation) m_animationBackend.reset(static_cast<IAnimationBackend*>(m_config.animation));


    detail::g_mainThreadId = std::this_thread::get_id();

    if (m_config.editorMode) {
        fprintf(stderr, "[Engine] Running in EDITOR mode (hidden window + GPU)\n");
    }
    if (m_config.headless) {
        fprintf(stderr, "[Engine] Running in HEADLESS mode (no window, no GPU)\n");
    }

    if (!DebugManager::instance().init("logs")) {
        fprintf(stderr, "[Engine] DebugManager init failed - continuing.\n");
    }

    // T2: Phase-split initialization
    if (!initPlatformPhase()) return false;
    if (!initScriptingPhase()) return false;
    if (!initAssetPhase()) return false;
    if (!initOptionalPhase()) {
        fprintf(stderr, "[Engine] Optional subsystems init had issues (non-fatal).\n");
    }

    DEBUG_INFO(SubSys::Engine, ErrCode::Ok, "All subsystems initialized.");
    return true;
}

// ============================================================================
// T2.1: Platform + Render + Audio initialization
// ============================================================================

bool Engine::initPlatformPhase() {
    int width  = m_config.width;
    int height = m_config.height;
    const char* title = m_config.title;

    if (!m_config.headless || m_config.editorMode || m_config.platform) {
        if (!m_platformBackend->init(title, width, height)) {
            DEBUG_ERROR(SubSys::Engine, ErrCode::Engine_PlatformInitFailed,
                        "SDL3 platform backend init failed.");
            return false;
        }
        BackendRegistry::instance().setPlatformBackend(*m_platformBackend);

        if (m_config.editorMode) {
            SDL_HideWindow(static_cast<SDL_Window*>(
                m_platformBackend->getNativeWindowHandle()));
        }

        void* nwh = m_platformBackend->getNativeWindowHandle();
        DEBUG_INFO(SubSys::Engine, ErrCode::Ok, "Native window handle: %p", nwh);

        if (!m_renderDevice->init(nwh, width, height)) {
            DEBUG_ERROR(SubSys::Engine, ErrCode::Engine_RenderInitFailed,
                        "Render device init failed.");
            return false;
        }
        BackendRegistry::instance().setRenderDevice(*m_renderDevice);
    } else {
        // Headless: register null backends for safe no-op operations
        BackendRegistry::instance().registerNullBackends();
    }

    // Render info (GPU caps)
    if (!m_config.headless || m_config.editorMode || m_config.platform) {
        const bgfx::Caps* caps = bgfx::getCaps();
        DebugManager::RenderInfo ri;
        ri.backendName = bgfx::getRendererName(caps->rendererType);
        ri.width = width; ri.height = height;
        ri.viewCount = 3; ri.shaderReady = true;
        DebugManager::instance().setRenderInfo(ri);
    }

    // Audio backend init
    if (!m_config.headless || m_config.editorMode || m_config.platform) {
        if (!m_audioBackend->init()) {
            DEBUG_ERROR(SubSys::Engine, ErrCode::Engine_AudioInitFailed,
                        "Audio backend init failed.");
            return false;
        }
        BackendRegistry::instance().setAudioBackend(*m_audioBackend);
    } else {
        printf("[Engine] Headless mode: skipping audio init\n");
    }

    DebugManager::AudioInfo ai;
    ai.initialized = true; ai.bgmBusReady = true;
    ai.voiceBusReady = true; ai.seBusReady = true; ai.globalVolume = 1.0f;
    DebugManager::instance().setAudioInfo(ai);

    // Input router
    BackendRegistry::instance().setInputRouter(m_inputRouter.get());
    DebugManager::InputInfo ii;
    ii.currentFocus = "KAG";
    DebugManager::instance().setInputInfo(ii);

    m_gpuMonitor = createGpuMonitor(m_config.headless);

    // SaveManager + TextureBudget + misc registrations
    SaveManager::instance().init("saves/");
    TextureBudget::instance().detect();
    BackendRegistry::instance().setTextureBudget(&TextureBudget::instance());
    BackendRegistry::instance().setTextureManager(&TextureManager::instance());
    TextureManager::instance().initialize();
    BackendRegistry::instance().setDebugManager(&DebugManager::instance());
    BackendRegistry::instance().setAsyncLoader(&AsyncLoader::instance());

    return true;
}

// ============================================================================
// T2.2: Lua scripting + bindings + HotReload
// ============================================================================

bool Engine::initScriptingPhase() {
    if (!m_lua->init()) {
        // Wire SoLoud to Lua before failing
        auto* soLoud = static_cast<SoLoudAudioEngine*>(m_audioBackend.get());
        if (soLoud) soLoud->setLuaState(m_lua->state());
        fprintf(stderr, "[Engine] Lua VM init failed.\n");
        return false;
    }

    // Phase G8-U1: install lua_Alloc hook for memory monitoring
    lua_setallocf(m_lua->state(), s_luaAllocFn, m_lua->state());
    BackendRegistry::instance().setLuaState(m_lua->state());
    BackendRegistry::instance().setVideoPlayer(m_videoPlayer.get());

    // Phase R2-U1: Push all backend pointers into Lua registry so binding
    // files (KAG, Render, DevCore, Unified, VFX) can resolve them without
    // including BackendRegistry.h.  Keys are the runtime contract between
    // this init block and the binding files.
    {
        lua_State* L = m_lua->state();
        lua_pushlightuserdata(L, m_renderDevice.get());
        lua_setfield(L, LUA_REGISTRYINDEX, "Caesura.RenderDevice");

        lua_pushlightuserdata(L, m_audioBackend.get());
        lua_setfield(L, LUA_REGISTRYINDEX, "Caesura.AudioBackend");

        lua_pushlightuserdata(L, m_platformBackend.get());
        lua_setfield(L, LUA_REGISTRYINDEX, "Caesura.PlatformBackend");

        lua_pushlightuserdata(L, m_inputRouter.get());
        lua_setfield(L, LUA_REGISTRYINDEX, "Caesura.InputRouter");

        lua_pushlightuserdata(L, m_videoPlayer.get());
        lua_setfield(L, LUA_REGISTRYINDEX, "Caesura.VideoPlayer");

        lua_pushlightuserdata(L, static_cast<ITextureManager*>(&TextureManager::instance()));
        lua_setfield(L, LUA_REGISTRYINDEX, "Caesura.TextureManager");

        lua_pushlightuserdata(L, static_cast<IAsyncLoader*>(&AsyncLoader::instance()));
        lua_setfield(L, LUA_REGISTRYINDEX, "Caesura.AsyncLoader");

        lua_pushlightuserdata(L, static_cast<IDebugManager*>(&DebugManager::instance()));
        lua_setfield(L, LUA_REGISTRYINDEX, "Caesura.DebugManager");
    }

    // HotReload for scripts/ directory
    HotReload::instance().init("scripts/", m_lua->state());

    // Shared FreeType (before any text/font subsystem uses it)
    if (!FreeTypeContext::instance().init()) {
        fprintf(stderr, "[Engine] FreeTypeContext init failed.\n");
        return false;
    }

    return true;
}

// ============================================================================
// T2.3: Job system + Asset pipeline
// ============================================================================

bool Engine::initAssetPhase() {
    // Parallel task system
    JobSystem::instance().init();
    BackendRegistry::instance().setJobSystem(&JobSystem::instance());
    m_videoPlayer->setJobSystem(JobSystem::instance());

    // Asset management
    AssetManager::instance().init();
    // Inject CARC providers (moved from AssetManager to break resource→archive cycle)
    {
        const char* carcFiles[] = {"data.carc", "game.carc", "patch.carc"};
        for (const char* fname : carcFiles) {
            auto reader = std::make_unique<carc::CARCReader>();
            if (reader->open(fname)) {
                AssetManager::instance().addProvider(
                    std::make_unique<carc::CarcAssetProvider>(std::move(reader)));
                printf("[Engine] Registered CARC: %s\n", fname);
            }
        }
    }
    AsyncLoader::instance().init();

    return true;
}

// ============================================================================
// T2.4: Optional subsystems (Steam, Crypto, MiniGame, Animation)
// ============================================================================

bool Engine::initOptionalPhase() {
    bool allOk = true;

    // Steam init (optional, no-op if SDK not present)
    if (m_steamBackend->init()) {
        registerSteamBinding(m_lua->state(), m_steamBackend.get());
    }

    // Crypto engine registration (moved OUT of Steam if-block - bug fix)
    BackendRegistry::instance().setCryptoEngine(&carc::CryptoEngine::instance());

    // 3D mini-game backend (bgfx)
    m_miniGameBackend->setRenderDevice(m_renderDevice.get());
    m_miniGameBackend->init();
    BackendRegistry::instance().setMiniGameBackend(m_miniGameBackend.get());
    {
        lua_State* L = m_lua->state();
        lua_pushlightuserdata(L, m_miniGameBackend.get());
        lua_setfield(L, LUA_REGISTRYINDEX, "Caesura.MiniGameBackend");
    }

    // Animation backend (Live2D or Null)
#ifdef CAESURA_HAS_LIVE2D
    if (!m_animationBackend) m_animationBackend = std::make_unique<Live2DBackend>();
#else
    if (!m_animationBackend) m_animationBackend = std::make_unique<NullAnimationBackend>();
#endif
    if (!m_animationBackend->init()) {

        m_animationBackend = std::make_unique<NullAnimationBackend>();
        m_animationBackend->init();
        allOk = false;
    }
    BackendRegistry::instance().setAnimationBackend(m_animationBackend.get());
#ifdef CAESURA_HAS_LIVE2D
    static_cast<Live2DBackend&>(*m_animationBackend).setRenderDevice(
        m_renderDevice.get());
#endif

    return allOk;
}
void Engine::run() {
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
        processEvents();

        // --- GPU device loss recovery check (before any per-frame rendering) ---
        if (BgfxDebugCallback::isDeviceLost()) {
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
        m_frameTime = dt;

        // -- Phase 8.1: HotReload check (per frame) -----------------------
        HotReload::instance().checkAndReload();

        // JobSystem main-thread callbacks + async load SDL events
        JobSystem::instance().pollMainThreadJobs();
        AsyncLoader::instance().poll();

        // -- Phase G8-U1: Lua memory budget check \(every frame\) ---------------
        lua_State* Lgc = m_lua->state();
        if (Lgc) {
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
            if (!m_luaPaused) {
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

            lua_pushinteger(L, static_cast<int>(gpuQ));
            lua_setglobal(L, "_CAESURA_GPU_QUALITY");
            lua_pushboolean(L, m_gpuMonitor->vfxEnabled() ? 1 : 0);
            lua_setglobal(L, "_CAESURA_VFX_ENABLED");
            lua_pushnumber(L, static_cast<lua_Number>(m_gpuMonitor->metrics().gpuTimeMs));
            lua_setglobal(L, "_CAESURA_GPU_TIME_MS");
            lua_pushnumber(L, static_cast<lua_Number>(m_gpuMonitor->metrics().rollingAvgMs));
            lua_setglobal(L, "_CAESURA_GPU_AVG_MS");
            lua_pushboolean(L, m_gpuMonitor->metrics().degraded ? 1 : 0);
            lua_setglobal(L, "_CAESURA_GPU_DEGRADED");
        }

        // Audio voice-complete edge detection
        if (m_audioBackend) {
            bool playing = m_audioBackend->isVoicePlaying();
            if (m_audioVoiceWasPlaying && !playing && L) {
                lua_pushboolean(L, 1);
                lua_setglobal(L, "_CAESURA_VOICE_COMPLETE");
            }
            m_audioVoiceWasPlaying = playing;
        }

        render();
        bgfx::frame();

        // -- Reserved: 3D mini-game update hook (CPU work, future JobSystem target) --
        if (m_miniGameBackend && m_miniGameBackend->isActive()) {
            m_miniGameBackend->update(static_cast<float>(dt));
            // U3.3: invoke Lua-side per-frame input callback if defined
            if (L && !m_luaPaused) {
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

    shutdown();
}

void Engine::processEvents() {
    // Steam callbacks every frame
    m_steamBackend->runCallbacks();
    // Pause input while Steam overlay is active
    if (m_steamBackend->isOverlayActive()) return;
    // Track 3: Reset Lua instruction budget each frame
    m_lua->resetInstructionBudget();

    // Headless/Editor mode: no SDL events, minimal sleep
    if (m_config.headless || m_config.editorMode) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        return;
    }
    lua_State* L = m_lua->state();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // -- GPU device reset from SDL (triggers recovery at next loop iteration) --
        if (event.type == SDL_EVENT_RENDER_DEVICE_RESET) {
            BgfxDebugCallback::flagDeviceLost();
        }

        // -- G8-U3: Async load completion (custom SDL event from AsyncLoader) --
        if (event.type == CAESURA_EVENT_ASYNC_LOAD && L) {
            auto* completed = static_cast<AsyncLoader::CompletedLoad*>(event.user.data1);
            if (completed) {
                uint32_t texId = 0;
                if (completed->success && completed->type == "texture" &&
                    !completed->rgba.empty() && completed->width > 0) {
                    CAESURA_ASSERT_MAIN_THREAD();
                    texId = TextureManager::instance().loadTextureFromRGBA(
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
            continue;
        }

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
                m_inputRouter->getFocus() == InputFocus::KAG && !m_luaPaused) {
                const char* handler = (event.button.button == SDL_BUTTON_RIGHT) ? "_KAG_onRightClick" : "_KAG_onClick";
                lua_getglobal(L, handler);
                if (lua_isfunction(L, -1)) {
                    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                        const char* err = lua_tostring(L, -1);
                        fprintf(stderr, "%s: %s\n", handler, err ? err : "unknown");
                        lua_pop(L, 1);
                    }
                } else { lua_pop(L, 1); }
            }

            // D9.7: Mouse wheel for backlog scrolling
            if (event.type == SDL_EVENT_MOUSE_WHEEL && m_inputRouter->getFocus() == InputFocus::KAG) {
                lua_pushnumber(L, event.wheel.y); lua_setglobal(L, "_KAG_MOUSE_WHEEL_Y");
                lua_getglobal(L, "_KAG_onMouseWheel");
                if (lua_isfunction(L, -1)) {
                    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                        fprintf(stderr, "_KAG_onMouseWheel: %s\n", lua_tostring(L, -1));
                        lua_pop(L, 1);
                    }
                } else { lua_pop(L, 1); }
            }
        }

        if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && L) {
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_F5) { lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_F5"); }
                if (event.key.key == SDLK_F6) { lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_F6"); }
                if (event.key.key == SDLK_W)    { lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_W"); }
                if (event.key.key == SDLK_A)    { lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_A"); }
                if (event.key.key == SDLK_S)    { lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_S"); }
                if (event.key.key == SDLK_D)    { lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_D"); }
                // D9.6: Ctrl triggers skip mode via Lua
                if (event.key.key == SDLK_LCTRL || event.key.key == SDLK_RCTRL) {
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
                // D9.6: Ctrl skip mode toggle release
                if (event.key.key == SDLK_LCTRL || event.key.key == SDLK_RCTRL) {
                    lua_getglobal(L, "_KAG_onCtrlUp");
                    if (lua_isfunction(L, -1)) {
                        if (lua_pcall(L, 0, 0, 0) != LUA_OK) { lua_pop(L, 1); }
                    } else { lua_pop(L, 1); }
                }
            }
        }
        m_inputRouter->processEvent(event);
    }

    if (!L) return;

    lua_getglobal(L, "_CAESURA_QUIT");
    if (lua_isboolean(L, -1) && lua_toboolean(L, -1)) {
        lua_pop(L, 1);
        m_running = false;
        return;
    }
    lua_pop(L, 1);
}

void Engine::render() {
    // Headless mode (not editor): no GPU rendering
    if (m_config.headless && !m_config.editorMode) return;

    m_lua->resetInstructionBudget();
    lua_State* L = m_lua->state();
    if (L && !m_luaPaused) {
        lua_getglobal(L, "engine_render");
        if (lua_isfunction(L, -1)) {
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                fprintf(stderr, "engine_render: %s\n", err ? err : "unknown");
                lua_pop(L, 1);
            }
        } else { lua_pop(L, 1); }
    }


    // bgfx debug text overlay: engine info + renderer + resolution
    const bgfx::Caps* caps = bgfx::getCaps();
    if (caps) {
        bgfx::dbgTextClear();
        bgfx::dbgTextPrintf(0, 0, 0x0F, "Caesura (AmeKAG) v1.0.0");
        bgfx::dbgTextPrintf(0, 1, 0x0F, "Renderer: %s  %dx%d",
                            bgfx::getRendererName(caps->rendererType), m_config.width, m_config.height);
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
    lua_State* L = m_lua->state(); if (!L) return;
    lua_getglobal(L, "quicksave");
    if (lua_isfunction(L, -1)) { if (lua_pcall(L, 0, 0, 0) != LUA_OK) { fprintf(stderr, "quicksave: %s\n", lua_tostring(L, -1)); lua_pop(L, 1); } }
    else { lua_pop(L, 1); }
}
void Engine::quickload() {
    lua_State* L = m_lua->state(); if (!L) return;
    lua_getglobal(L, "quickload");
    if (lua_isfunction(L, -1)) { if (lua_pcall(L, 0, 0, 0) != LUA_OK) { fprintf(stderr, "quickload: %s\n", lua_tostring(L, -1)); lua_pop(L, 1); } }
    else { lua_pop(L, 1); }
}
void Engine::triggerAutoSave() {
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
            HotReload::instance().requestReload();
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
    if (m_config.headless && !m_config.editorMode) return;
    lua_State* L = m_lua->state();
    if (L && !m_luaPaused) {
        lua_getglobal(L, "engine_update");
        if (lua_isfunction(L, -1)) {
            lua_pushnumber(L, 0.016);
            if (lua_pcall(L, 1, 0, 0) != LUA_OK) { lua_pop(L, 1); }
        } else { lua_pop(L, 1); }
    }
    render();
    bgfx::frame();
}

static std::string captureFrameBase64(int w, int h) {
    static int counter = 0;
    char path[256];
    snprintf(path, sizeof(path), "editor_frame_%d.png", counter++);
    if (counter > 99) counter = 0;
    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path);
    bgfx::frame();
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return "";
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();
    std::remove(path);
    static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((size + 2) / 3) * 4);
    for (size_t i = 0; i < static_cast<size_t>(size); i += 3) {
        unsigned char a = buffer[i];
        unsigned char b = (i + 1 < size) ? buffer[i + 1] : 0;
        unsigned char c = (i + 2 < size) ? buffer[i + 2] : 0;
        result += b64[a >> 2];
        result += b64[((a & 3) << 4) | (b >> 4)];
        result += (i + 1 < size) ? b64[((b & 15) << 2) | (c >> 6)] : '=';
        result += (i + 2 < size) ? b64[c & 63] : '=';
    }
    return result;
}

std::string Engine::captureFrameForRpc(int w, int h) {
    // Render one frame first (submits draw calls without bgfx::frame)
    if (!m_config.headless) {
        lua_State* L = m_lua->state();
        if (L && !m_luaPaused) {
            lua_getglobal(L, "engine_update");
            if (lua_isfunction(L, -1)) {
                lua_pushnumber(L, 0.016);
                if (lua_pcall(L, 1, 0, 0) != LUA_OK) { lua_pop(L, 1); }
            } else { lua_pop(L, 1); }
        }
        render(); // submits draw calls, no bgfx::frame yet
    }
    // captureFrameBase64 calls requestScreenShot + bgfx::frame,
    // which triggers the screenshot of the frame we just rendered.
    return captureFrameBase64(w, h);
}

void Engine::runRpc() {
    fprintf(stderr, "[Engine] Starting JSON-RPC loop (stdin/stdout)\n");
    RpcServer::instance().setLuaState(m_lua->state());
    RpcServer::instance().setFrameCaptureCallback([this](int w, int h) { return captureFrameForRpc(w, h); });
    RpcServer::instance().run();
}

void Engine::shutdown() {
    if (m_shutdownComplete) return;
    m_shutdownComplete = true;

    // Signal bgfx debug callback that shutdown is in progress
    // (suppresses FATAL/WARN noise during GPU resource teardown)
    setBgfxShuttingDown(true);

    // Reset error crash counters on clean shutdown
    ErrorUI::resetCounters();

    if (m_miniGameBackend) { m_miniGameBackend->shutdown(); m_miniGameBackend.reset(); }

    AsyncLoader::instance().shutdown();
    AssetManager::instance().shutdown();
    JobSystem::instance().shutdown();
    unregisterSteamBinding(m_steamBackend.get());
    if (m_steamBackend) m_steamBackend->shutdown();
    if (m_videoPlayer) m_videoPlayer->shutdown();
    if (m_lua) m_lua->shutdown();
    if (m_audioBackend) m_audioBackend->shutdown();

    // Flush bgfx pipeline BEFORE touching any D3D11-shared resources (Live2D)
    if (m_renderDevice) { m_renderDevice->flushAllRTT(); }

    // Process one final bgfx frame to drain pending GPU commands
    if (m_renderDevice) { bgfx::frame(); }

    // Animation (Live2D) shut down after bgfx pipeline is drained
    if (m_animationBackend) { m_animationBackend->shutdown(); m_animationBackend.reset(); }

    // Final bgfx frame after Live2D cleanup
    if (m_renderDevice) { bgfx::frame(); }
    
    if (m_renderDevice) { m_renderDevice->shutdown(); }
    FreeTypeContext::instance().shutdown();
    if (m_platformBackend) m_platformBackend->shutdown();
    DebugManager::instance().shutdown();
}

void Engine::recoverFromDeviceLoss() {
    fprintf(stderr, "[Engine] === GPU device lost — starting recovery ===\n");
    m_luaPaused = true;

    // Phase 1: Notify all listeners to release GPU resources
    BackendRegistry::instance().notifyDeviceLost();

    // Phase 2: Shutdown bgfx
    setBgfxShuttingDown(true);
    bgfx::shutdown();
    setBgfxShuttingDown(false);
    fprintf(stderr, "[Engine] bgfx shutdown complete\n");

    // Phase 3: Reinitialize bgfx
    void* nwh = nullptr;
    if (m_platformBackend) {
        nwh = m_platformBackend->getNativeWindowHandle();
    }
    int w = m_platformBackend ? m_platformBackend->getWindowWidth() : m_config.width;
    int h = m_platformBackend ? m_platformBackend->getWindowHeight() : m_config.height;

    bgfx::Init initCfg;
    initCfg.type = bgfx::RendererType::Count; // auto-detect
    initCfg.vendorId = BGFX_PCI_ID_NONE;
    initCfg.platformData.nwh = nwh;
    initCfg.resolution.width = uint32_t(w);
    initCfg.resolution.height = uint32_t(h);
    initCfg.resolution.reset = BGFX_RESET_VSYNC;

    if (!bgfx::init(initCfg)) {
        fprintf(stderr, "[Engine] FATAL: bgfx reinit failed after device loss\n");
        handleFatalError("GPU Recovery", "bgfx::init() failed after device loss");
        return;
    }
    fprintf(stderr, "[Engine] bgfx reinit success (%dx%d)\n", w, h);

    // Phase 4: Notify all listeners to recreate GPU resources
    BackendRegistry::instance().notifyDeviceRestored();

    // Phase 5: Notify Lua scripts to reload textures
    lua_State* L = m_lua->state();
    if (L) {
        lua_pushboolean(L, 1);
        lua_setglobal(L, "_CAESURA_DEVICE_RESTORED");
    }

    m_luaPaused = false;
    m_deviceLostRecovery = false;
    fprintf(stderr, "[Engine] === GPU device recovery complete ===\n");
}

} // namespace Caesura



