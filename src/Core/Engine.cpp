extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include "Engine.h"
#include "InputRouter.h"
#include "BackendRegistry.h"
#include "DebugManager.h"
#include "SDL3PlatformBackend.h"
#include "IAudioBackend.h"
#include "../Render/IRenderDevice.h"
#include "../Audio/SoLoudAudioEngine.h"
#include "../Scripting/RenderBinding.h"
#include "../Render/BgfxRenderDevice.h"
#include "../Scripting/LuaManager.h"
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <bx/bx.h>
#include <cstdio>
#include <cmath>

namespace Caesura {

Engine::Engine()
    : m_lua(std::make_unique<LuaManager>())
    , m_inputRouter(std::make_unique<InputRouter>())
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

    // Note: bgfx::reset() is NOT called here because it destroys
    // all view settings configured by BgfxRenderDevice::setupDefaultViews().
    // Resolution is already set via bgfx::init() params.


    if (!m_lua->init()) {
        fprintf(stderr, "Lua VM init failed.");
        return false;
    }

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

        lua_State* L = m_lua->state();
        if (L) {
            lua_getglobal(L, "engine_update");
            if (lua_isfunction(L, -1)) {
                lua_pushnumber(L, dt);
                if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                    fprintf(stderr, "engine_update: %s",
                            lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown");
                    lua_pop(L, 1);
                }
            } else { lua_pop(L, 1); }
        }

        // Audio update before beginFrame (P0 fix: correct frame order)
        if (m_audioBackend) m_audioBackend->update(dt);

        if (m_renderDevice) {
            m_renderDevice->beginFrame();
            render();
            m_renderDevice->endFrame();
        }
    }
}

void Engine::quit() { m_running = false; }

void Engine::processEvents() {
    lua_State* L = m_lua->state();
    auto* sdlBackend = dynamic_cast<SDL3PlatformBackend*>(m_platformBackend.get());
    if (!sdlBackend) { m_running = false; return; }
    while (sdlBackend->pollEvent()) {
        SDL_Event& event = sdlBackend->lastEvent();
        if (event.type == SDL_EVENT_QUIT) { m_running = false; return; }
        // Handle window resize
        if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            int newW = (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
                ? (int)event.window.data1 : (int)event.window.data1;
            int newH = (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
                ? (int)event.window.data2 : (int)event.window.data2;
            if (newW > 0 && newH > 0 && (newW != m_width || newH != m_height)) {
                m_width  = newW;
                m_height = newH;
                if (m_renderDevice) m_renderDevice->resize(newW, newH);
                // Notify InputRouter → layers.lua to rebuild layer tree and mark all layers dirty
                m_inputRouter->notifyResize(newW, newH);
                // Set Lua globals so layers.lua can detect resize and rebuild
                if (L) {
                    lua_pushinteger(L, newW); lua_setglobal(L, "_CAESURA_WINDOW_W");
                    lua_pushinteger(L, newH); lua_setglobal(L, "_CAESURA_WINDOW_H");
                    lua_pushboolean(L, 1); lua_setglobal(L, "_CAESURA_RESIZED");
                }
                DEBUG_INFO(SubSys::Engine, ErrCode::Ok, "Window resized: %dx%d", newW, newH);
            }
            continue;
        }

        if (L) {
            auto mouse = m_platformBackend->getMouseState();
            // P1 security: clamp mouse coords to window boundaries
            if (mouse.x < 0) mouse.x = 0;
            if (mouse.y < 0) mouse.y = 0;
            if (mouse.x > m_width) mouse.x = (float)m_width;
            if (mouse.y > m_height) mouse.y = (float)m_height;
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
                        fprintf(stderr, "_KAG_onClick: %s",
                                lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown");
                        lua_pop(L, 1);
                    }
                } else { lua_pop(L, 1); }
            }
        }
        m_inputRouter->processEvent(event);
    }

    // P0 fix: null pointer check before _CAESURA_QUIT
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
                fprintf(stderr, "engine_render: %s",
                        lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown");
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
                    fprintf(stderr, "Engine_OnFrameRender: %s",
                            lua_tostring(L2, -1) ? lua_tostring(L2, -1) : "unknown");
                    lua_pop(L2, 1);
                }
            } else { lua_pop(L2, 1); }
        }
    }

    // P1 fix: removed empty-frame bgfx::touch calls
    // Views are touched by the render bindings that actually draw into them.

    const bgfx::Caps* caps = bgfx::getCaps();
    if (caps) {
        bgfx::dbgTextClear();
        bgfx::dbgTextPrintf(0, 0, 0x0F, "Caesura (AmeKAG) v1.0.0");
        bgfx::dbgTextPrintf(0, 1, 0x0F, "Renderer: %s  %dx%d",
                            bgfx::getRendererName(caps->rendererType), m_width, m_height);
        bgfx::dbgTextPrintf(0, 2, 0x0F, "Input Focus: %s  Errors: %u",
                            inputFocusToString(m_inputRouter->getFocus()), 0);
        bgfx::dbgTextPrintf(0, 3, 0x0F, "Log: %s", "logs/");
    }
}

void Engine::shutdown() {
    if (m_shutdownComplete) return;
    m_shutdownComplete = true;
    if (m_lua) m_lua->shutdown();
    if (m_renderDevice) { m_renderDevice->flushAllRTT(); m_renderDevice->shutdown(); }
    if (m_audioBackend) m_audioBackend->shutdown();
    if (m_platformBackend) m_platformBackend->shutdown();
    DebugManager::instance().shutdown();
}

} // namespace Caesura
