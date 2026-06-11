extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include "Engine.h"
#include "InputRouter.h"
#include "BackendRegistry.h"
#include "Debug/DebugManager.h"
#include "RpcServer.h"
#include "ErrorUI.h"
#include "SDL3PlatformBackend.h"
#include "IAudioBackend.h"
#include "TextureBudget.h"
#include "Render/IRenderDevice.h"
#include "Render/GpuMonitor.h"
#include "Render/VideoPlayer.h"
#include "Render/FreeTypeContext.h"
#include "Audio/SoLoudAudioEngine.h"
#include "Scripting/RenderBinding.h"
#include "Render/BgfxRenderDevice.h"
#include "Scripting/LuaManager.h"
#include "System/SaveManager.h"
#include "Debug/HotReload.h"
#include "JobSystem.h"
#include "Resource/AsyncLoader.h"
#include "Resource/AssetManager.h"
#include "Render/TextureManager.h"
#include "MiniGame/BgfxMiniGameBackend.h"
#include "../Live2D/NullAnimationBackend.h"
#include "../Steam/ISteamBackend.h"
#include "../Steam/SteamBackend.h"
#include "../Steam/NullSteamBackend.h"
#include "../Scripting/SteamBinding.h"
#ifdef CAESURA_HAS_LIVE2D
#include "../Live2D/Live2D/Live2DBackend.h"
#include "../Render/BgfxRenderDevice.h"
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


std::thread::id Caesura::Engine::s_mainThreadId;

namespace Caesura {

// -- Phase G8-U1: lua_Alloc hook for per-allocation memory check ---------------
// Checks lua_gc(L, LUA_GCCOUNT) after every Lua allocation.
// Returns NULL on overflow to force a Lua allocation error.
static void* s_luaAllocFn(void* ud, void* ptr, size_t osize, size_t nsize) {
    // Lua 5.4 forbids calling lua_gc inside the allocator (C stack overflow).
    if (nsize == 0) { free(ptr); return nullptr; }
    return realloc(ptr, nsize);
}

Engine::Engine()
    : m_lua(std::make_unique<LuaManager>())
    , m_inputRouter(std::make_unique<InputRouter>())
    , m_gpuMonitor(std::make_unique<GpuMonitor>())
    , m_videoPlayer(std::make_unique<VideoPlayer>())
#ifdef CAESURA_HAS_STEAM
    , m_steamBackend(std::make_unique<SteamBackend>())
#else
    , m_steamBackend(std::make_unique<NullSteamBackend>())
#endif
    
{}

Engine::~Engine() {
    if (!m_shutdownComplete) shutdown();
}

IRenderDevice& Engine::renderDevice() { return *BackendRegistry::instance().getRenderDevice(); }
IAudioBackend& Engine::audio() { return *BackendRegistry::instance().getAudioBackend(); }
IPlatformBackend& Engine::platform() { return *BackendRegistry::instance().getPlatformBackend(); }

bool Engine::init(const char* title, int width, int height, bool headless, bool editorMode) {
    s_mainThreadId = std::this_thread::get_id();
    m_width  = width;
    m_height = height;

    m_headless = headless;
    m_editorMode = editorMode;
    if (m_editorMode) {
        fprintf(stderr, "[Engine] Running in EDITOR mode (hidden window + GPU)\n");
    }
    if (m_headless) {
        fprintf(stderr, "[Engine] Running in HEADLESS mode (no window, no GPU)\n");
    }

    if (!DebugManager::instance().init("logs")) {
        fprintf(stderr, "[Engine] DebugManager init failed - continuing.\n");
    }

    if (!m_headless || m_editorMode) {
    m_platformBackend = std::make_unique<SDL3PlatformBackend>();
    if (!m_platformBackend->init(title, width, height)) {
        fprintf(stderr, "SDL3 platform backend init failed.");
        return false;
    }
    BackendRegistry::instance().setPlatformBackend(*m_platformBackend);
    if (m_editorMode) {
        SDL_HideWindow(static_cast<SDL_Window*>(m_platformBackend->getNativeWindowHandle()));
    }

    void* nwh = m_platformBackend->getNativeWindowHandle();
    DEBUG_INFO(SubSys::Engine, ErrCode::Ok, "Native window handle: %p", nwh);

    m_renderDevice = std::make_unique<BgfxRenderDevice>();
    if (!m_renderDevice->init(nwh, width, height)) {
        fprintf(stderr, "Render device init failed.");
        return false;
    }
    BackendRegistry::instance().setRenderDevice(*m_renderDevice);
    } else {
        // Headless: register null backends for safe no-op operations
        BackendRegistry::instance().registerNullBackends();
    }

    if (!m_headless || m_editorMode) {
    const bgfx::Caps* caps = bgfx::getCaps();
    DebugManager::RenderInfo ri;
    ri.backendName = bgfx::getRendererName(caps->rendererType);
    ri.width = width; ri.height = height;
    ri.viewCount = 3; ri.shaderReady = true;
    DebugManager::instance().setRenderInfo(ri);
    }

    m_audioBackend = std::make_unique<SoLoudAudioEngine>();
    if (!m_audioBackend->init()) {
        fprintf(stderr, "Audio backend init failed.");
        return false;
    }
    BackendRegistry::instance().setAudioBackend(*m_audioBackend);

    DebugManager::AudioInfo ai;
    ai.initialized = true; ai.bgmBusReady = true;
    ai.voiceBusReady = true; ai.seBusReady = true; ai.globalVolume = 1.0f;
    DebugManager::instance().setAudioInfo(ai);

    BackendRegistry::instance().setInputRouter(m_inputRouter.get());

    DebugManager::InputInfo ii;
    ii.currentFocus = "KAG";
    DebugManager::instance().setInputInfo(ii);

    SaveManager::instance().init("saves/");

    TextureBudget::instance().detect();
    BackendRegistry::instance().setTextureBudget(&TextureBudget::instance());

    if (!m_lua->init()) {
        fprintf(stderr, "Lua VM init failed.");
        return false;
    }

    // Phase G8-U1: install lua_Alloc hook for memory monitoring
    lua_setallocf(m_lua->state(), s_luaAllocFn, m_lua->state());

    BackendRegistry::instance().setVideoPlayer(m_videoPlayer.get());

    // -- Phase 8.1: Initialize HotReload for scripts/ directory -----------
    HotReload::instance().init("scripts/", m_lua->state());

    // Initialize shared FreeType (before any text/font subsystem uses it)
    if (!FreeTypeContext::instance().init()) {
        fprintf(stderr, "[Engine] FreeTypeContext init failed.\n");
        return false;
    }

    // Parallel task system + asset pipeline
    JobSystem::instance().init();
    AssetManager::instance().init();
    AsyncLoader::instance().init();

    // Steam init (optional, no-op if SDK not present)
    if (m_steamBackend->init()) {
        registerSteamBinding(m_lua->state(), m_steamBackend.get());
    }

        // -- 3D mini-game backend (bgfx) --
    m_miniGameBackend = std::make_unique<BgfxMiniGameBackend>();
    m_miniGameBackend->setRenderDevice(m_renderDevice.get());
    m_miniGameBackend->init();
    BackendRegistry::instance().setMiniGameBackend(m_miniGameBackend.get());

    // -- Animation backend (Live2D or Null) --
#ifdef CAESURA_HAS_LIVE2D
    m_animationBackend = std::make_unique<Live2DBackend>();
#else
    m_animationBackend = std::make_unique<NullAnimationBackend>();
#endif
    if (!m_animationBackend->init()) {
        fprintf(stderr, "Animation backend init failed, falling back to null.\n");
        m_animationBackend = std::make_unique<NullAnimationBackend>();
        m_animationBackend->init();
    }
    BackendRegistry::instance().setAnimationBackend(m_animationBackend.get());
#ifdef CAESURA_HAS_LIVE2D
    static_cast<Live2DBackend&>(*m_animationBackend).setRenderDevice(
        m_renderDevice.get());
#endif

    DEBUG_INFO(SubSys::Engine, ErrCode::Ok, "All subsystems initialized.");
    return true;
}

void Engine::run() {
    m_running = true;
    if (m_headless) {
        m_lastTick = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    } else {
        m_lastTick = m_platformBackend->getTicksMs();
    }
    DEBUG_INFO(SubSys::Engine, ErrCode::Ok, "Entering main loop.");

    while (m_running) {
        PROFILE_SCOPE("frame");
        processEvents();

        uint64_t now;
        if (m_headless) {
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
                    if (lua_isfunction(Lgc, -1)) lua_pcall(Lgc, 0, 0, 0);
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
    if (m_headless || m_editorMode) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        return;
    }
    lua_State* L = m_lua->state();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
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

            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_F5) { lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_F5"); }
                if (event.key.key == SDLK_F6) { lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_F6"); }
            }
            if (event.type == SDL_EVENT_KEY_UP) {
                if (event.key.key == SDLK_F5) { lua_pushboolean(L, 0); lua_setglobal(L, "_GAME_KEY_F5"); }
                if (event.key.key == SDLK_F6) { lua_pushboolean(L, 0); lua_setglobal(L, "_GAME_KEY_F6"); }
            }

            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                m_inputRouter->getFocus() == InputFocus::KAG) {
                lua_getglobal(L, "_KAG_onClick");
                if (lua_isfunction(L, -1)) {
                    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                        const char* err = lua_tostring(L, -1);
                        fprintf(stderr, "_KAG_onClick: %s\n", err ? err : "unknown");
                        lua_pop(L, 1);
                    }
                } else { lua_pop(L, 1); }
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
    if (m_headless && !m_editorMode) return;

    m_lua->resetInstructionBudget();
    lua_State* L = m_lua->state();
    if (L) {
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
                            bgfx::getRendererName(caps->rendererType), m_width, m_height);
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
    if (m_headless && !m_editorMode) return;
    lua_State* L = m_lua->state();
    if (L) {
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
    if (!m_headless) {
        lua_State* L = m_lua->state();
        if (L) {
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
    bgfx::frame();

    // Animation (Live2D) shut down after bgfx pipeline is drained
    if (m_animationBackend) { m_animationBackend->shutdown(); m_animationBackend.reset(); }

    // Final bgfx frame after Live2D cleanup
    bgfx::frame();
    
    if (m_renderDevice) { m_renderDevice->shutdown(); }
    FreeTypeContext::instance().shutdown();
    if (m_platformBackend) m_platformBackend->shutdown();
    DebugManager::instance().shutdown();
}

} // namespace Caesura



