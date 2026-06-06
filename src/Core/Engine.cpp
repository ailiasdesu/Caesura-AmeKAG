extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include "Engine.h"
#include "InputRouter.h"
#include "BackendRegistry.h"
#include "DebugManager.h"
#include "ErrorUI.h"
#include "SDL3PlatformBackend.h"
#include "IAudioBackend.h"
#include "../Render/IRenderDevice.h"
#include "../Render/GpuMonitor.h"
#include "../Render/VideoPlayer.h"
#include "../Audio/SoLoudAudioEngine.h"
#include "../Scripting/RenderBinding.h"
#include "../Render/BgfxRenderDevice.h"
#include "../Scripting/LuaManager.h"
#include "../System/SaveManager.h"
#include "../Debug/HotReload.h"
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <bx/bx.h>
#include <cstdio>
#include <cmath>

namespace Caesura {

Engine::Engine()
    : m_lua(std::make_unique<LuaManager>())
    , m_inputRouter(std::make_unique<InputRouter>())
    , m_gpuMonitor(std::make_unique<GpuMonitor>())
    , m_videoPlayer(std::make_unique<VideoPlayer>())
    
{}

Engine::~Engine() {
    if (!m_shutdownComplete) shutdown();
}

IRenderDevice& Engine::renderDevice() { return *BackendRegistry::instance().getRenderDevice(); }
IAudioBackend& Engine::audio() { return *BackendRegistry::instance().getAudioBackend(); }
IPlatformBackend& Engine::platform() { return *BackendRegistry::instance().getPlatformBackend(); }

bool Engine::init(const char* title, int width, int height) {
    m_width  = width;
    m_height = height;

    if (!DebugManager::instance().init("logs")) {
        fprintf(stderr, "[Engine] DebugManager init failed - continuing.\n");
    }

    m_platformBackend = std::make_unique<SDL3PlatformBackend>();
    if (!m_platformBackend->init(title, width, height)) {
        fprintf(stderr, "SDL3 platform backend init failed.");
        return false;
    }
    BackendRegistry::instance().setPlatformBackend(m_platformBackend.get());

    void* nwh = m_platformBackend->getNativeWindowHandle();
    DEBUG_INFO(SubSys::Engine, ErrCode::Ok, "Native window handle: %p", nwh);

    m_renderDevice = std::make_unique<BgfxRenderDevice>();
    if (!m_renderDevice->init(nwh, width, height)) {
        fprintf(stderr, "Render device init failed.");
        return false;
    }
    BackendRegistry::instance().setRenderDevice(m_renderDevice.get());

    const bgfx::Caps* caps = bgfx::getCaps();
    DebugManager::RenderInfo ri;
    ri.backendName = bgfx::getRendererName(caps->rendererType);
    ri.width = width; ri.height = height;
    ri.viewCount = 3; ri.shaderReady = true;
    DebugManager::instance().setRenderInfo(ri);

    m_audioBackend = std::make_unique<SoLoudAudioEngine>();
    if (!m_audioBackend->init()) {
        fprintf(stderr, "Audio backend init failed.");
        return false;
    }
    BackendRegistry::instance().setAudioBackend(m_audioBackend.get());

    DebugManager::AudioInfo ai;
    ai.initialized = true; ai.bgmBusReady = true;
    ai.voiceBusReady = true; ai.seBusReady = true; ai.globalVolume = 1.0f;
    DebugManager::instance().setAudioInfo(ai);

    BackendRegistry::instance().setInputRouter(m_inputRouter.get());

    DebugManager::InputInfo ii;
    ii.currentFocus = "KAG";
    DebugManager::instance().setInputInfo(ii);

    SaveManager::instance().init("saves/");

    if (!m_lua->init()) {
        fprintf(stderr, "Lua VM init failed.");
        return false;
    }

    BackendRegistry::instance().setVideoPlayer(m_videoPlayer.get());

    // -- Phase 8.1: Initialize HotReload for scripts/ directory -----------
    HotReload::instance().init("scripts/", m_lua->state());

    DEBUG_INFO(SubSys::Engine, ErrCode::Ok, "All subsystems initialized.");
    return true;
}

void Engine::run() {
    m_running = true;
    m_lastTick = m_platformBackend->getTicksMs();
    DEBUG_INFO(SubSys::Engine, ErrCode::Ok, "Entering main loop.");

    while (m_running) {
        PROFILE_SCOPE("frame");
        processEvents();

        uint64_t now = m_platformBackend->getTicksMs();
        float dt = (float)(now - m_lastTick) / 1000.0f;
        m_lastTick = now;
        if (dt < 0.0f) dt = 0.0f;
        if (dt > 0.25f) dt = 0.25f;

        // -- Phase 8.1: HotReload check (per frame) -----------------------
        HotReload::instance().checkAndReload();

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
    }

    shutdown();
}

void Engine::processEvents() {
    lua_State* L = m_lua->state();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            m_running = false;
            return;
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

    if (m_inputRouter->getFocus() == InputFocus::GAME) {
        lua_State* L2 = m_lua->state();
        if (L2) {
            lua_getglobal(L2, "Engine_OnFrameRender");
            if (lua_isfunction(L2, -1)) {
                if (lua_pcall(L2, 0, 0, 0) != LUA_OK) {
                    fprintf(stderr, "Engine_OnFrameRender: %s\n",
                            lua_tostring(L2, -1) ? lua_tostring(L2, -1) : "unknown");
                    lua_pop(L2, 1);
                }
            } else { lua_pop(L2, 1); }
        }
    }

    const bgfx::Caps* caps = bgfx::getCaps();
    if (caps) {
        bgfx::dbgTextClear();
        bgfx::dbgTextPrintf(0, 0, 0x0F, "Caesura (AmeKAG) v1.0.0");
        bgfx::dbgTextPrintf(0, 1, 0x0F, "Renderer: %s  %dx%d",
                            bgfx::getRendererName(caps->rendererType), m_width, m_height);
        bgfx::dbgTextPrintf(0, 2, 0x0F, "Input Focus: %s  Errors: %u",
                            inputFocusToString(m_inputRouter->getFocus()), 0);
        const auto& gm = m_gpuMonitor->metrics();
        bgfx::dbgTextPrintf(0, 3, 0x0F, "GPU: %s | frame=%.1fms avg=%.1fms | degradation=%s",
                            gpuQualityName(gm.quality), gm.gpuTimeMs, gm.rollingAvgMs,
                            gm.degraded ? "ACTIVE" : "none");
        bgfx::dbgTextPrintf(0, 4, 0x0F, "Videos: %d  Log: %s",
                            m_videoPlayer->activeCount(), "logs/");
    }
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

void Engine::shutdown() {
    if (m_shutdownComplete) return;
    m_shutdownComplete = true;

    // Reset error crash counters on clean shutdown
    ErrorUI::resetCounters();

    if (m_lua) m_lua->shutdown();
    if (m_videoPlayer) m_videoPlayer->shutdown();
    if (m_renderDevice) { m_renderDevice->flushAllRTT(); m_renderDevice->shutdown(); }
    if (m_audioBackend) m_audioBackend->shutdown();
    if (m_platformBackend) m_platformBackend->shutdown();
    DebugManager::instance().shutdown();
}

} // namespace Caesura