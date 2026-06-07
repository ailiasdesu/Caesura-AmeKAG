extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "BackendRegistry.h"
#include "SDL3PlatformBackend.h"
#include "../Render/BgfxRenderDevice.h"
#include "../Audio/SoLoudAudioEngine.h"
#include "../Audio/NullAudioBackend.h"
#include <cstdio>
#include <cstring>

namespace Caesura {

// ============================================================================
//  Null backends -- safe no-op fallbacks when a backend is unavailable
// ============================================================================

class NullRenderDevice : public IRenderDevice {
public:
    NullRenderDevice() { printf("[BackendRegistry] Using NullRenderDevice.\n"); }
    bool init(void*, int w, int h) override { m_width = w; m_height = h; return true; }
    void shutdown() override {}
    void flushAllRTT() override {}
    void beginFrame() override {}
    void endFrame() override {}
    void commit_frame() override {}
    void setViewRect(uint16_t, uint16_t, uint16_t, uint16_t, uint16_t) override {}
    void setViewClear(uint16_t, uint16_t, uint32_t, float, uint8_t) override {}
    void touch(uint16_t) override {}
    ViewportHandle createRenderTarget(int, int) override { return ViewportHandle{}; }
    void destroyRenderTarget(ViewportHandle) override {}
    void blitViewport(ViewportHandle, uint16_t, float, float, float, float) override {}
    int getBackbufferWidth() const override { return m_width; }
    int getBackbufferHeight() const override { return m_height; }
    void resize(int w, int h) override { m_width = w; m_height = h; }
    void blitTexture(uint16_t, uint32_t, float, float, float, float, uint8_t) override {}
    void setDebugName(uint16_t, const std::string&) override {}
    void renderText(uint16_t, const std::string&, float, float, uint8_t, uint8_t, uint8_t, uint8_t) override {}
    void renderRuby(uint16_t, const std::string&, const std::string&, float, float, uint8_t, uint8_t, uint8_t, uint8_t) override {}
    void setFont(int) override {}
    void stretchBlt(uint16_t, uint32_t, float, float, float, float,
                    uint32_t, float, float, float, float, int) override {}
    void affineBlt(uint16_t, uint32_t, float, float, float, float,
                   uint32_t, float, float, float, float,
                   const float*) override {}
    void beginBatch() override {}
    void flushBatch() override {}
    float textLineHeight() const override { return 0.0f; }
private:
    int m_width = 0, m_height = 0;
};

class NullPlatformBackend : public IPlatformBackend {
public:
    NullPlatformBackend() { printf("[BackendRegistry] Using NullPlatformBackend.\n"); }
    bool init(const char*, int w, int h) override { m_width = w; m_height = h; return true; }
    void shutdown() override {}
    bool pollEvent() override { return false; }
    MouseState getMouseState() const override { return MouseState{}; }
    uint64_t getTicksMs() const override { return 0; }
    void* getNativeWindowHandle() const override { return nullptr; }
    int getWindowWidth() const override { return m_width; }
    int getWindowHeight() const override { return m_height; }
    void setFullscreen(bool) override {}
    const char* getBackendName() const override { return "NullPlatform"; }
private:
    int m_width = 0, m_height = 0;
};

// -- Singleton -------------------------------------------------------------

BackendRegistry& BackendRegistry::instance() {
    static BackendRegistry inst;
    return inst;
}

// -- Set active backends ---------------------------------------------------

void BackendRegistry::setRenderDevice(IRenderDevice& device) { m_renderDevice = &device; }

void BackendRegistry::setAudioBackend(IAudioBackend& backend) { m_audioBackend = &backend; }

void BackendRegistry::setPlatformBackend(IPlatformBackend& backend) { m_platformBackend = &backend; }

void BackendRegistry::setInputRouter(InputRouter* router) {
    m_inputRouter = router;
}

void BackendRegistry::setVideoPlayer(VideoPlayer* player) {
    m_videoPlayer = player;
}

void BackendRegistry::setTextureManager(TextureManager* mgr) {
    m_textureManager = mgr;
}

void BackendRegistry::setLayerManager(LayerManager* mgr) {
    m_layerManager = mgr;
}

// -- Backend factory -------------------------------------------------------

IAudioBackend* BackendRegistry::createAudioBackend(const char* name) {
    if (strcmp(name, "soloud") == 0 || strcmp(name, "SoLoud") == 0) {
        m_audioBackend = new SoLoudAudioEngine();
        // Engine is responsible for lifecycle
        printf("[BackendRegistry] Created audio backend: SoLoud\n");
        return m_audioBackend;
    }
    if (strcmp(name, "null") == 0 || strcmp(name, "Null") == 0) {
        m_audioBackend = new NullAudioBackend();
        // Engine is responsible for lifecycle
        return m_audioBackend;
    }
    fprintf(stderr, "[BackendRegistry] Unknown audio backend: %s\n", name);
    return nullptr;
}

IRenderDevice* BackendRegistry::createRenderDevice(const char* name) {
    if (strcmp(name, "bgfx") == 0) {
        m_renderDevice = new BgfxRenderDevice();
        // Engine is responsible for lifecycle
        printf("[BackendRegistry] Created render backend: bgfx\n");
        return m_renderDevice;
    }
    if (strcmp(name, "null") == 0 || strcmp(name, "Null") == 0) {
        m_renderDevice = new NullRenderDevice();
        // Engine is responsible for lifecycle
        return m_renderDevice;
    }
    fprintf(stderr, "[BackendRegistry] Unknown render backend: %s\n", name);
    return nullptr;
}

IPlatformBackend* BackendRegistry::createPlatformBackend(const char* name) {
    if (strcmp(name, "sdl3") == 0 || strcmp(name, "SDL3") == 0) {
        m_platformBackend = new SDL3PlatformBackend();
        // Engine is responsible for lifecycle
        printf("[BackendRegistry] Created platform backend: SDL3\n");
        return m_platformBackend;
    }
    if (strcmp(name, "null") == 0 || strcmp(name, "Null") == 0) {
        m_platformBackend = new NullPlatformBackend();
        // Engine is responsible for lifecycle
        return m_platformBackend;
    }
    fprintf(stderr, "[BackendRegistry] Unknown platform backend: %s\n", name);
    return nullptr;
}

// -- Registry keys for Lua lightuserdata ----------------------------------

static const char* kRegistryKey_RenderDevice   = "Caesura.RenderDevice";
static const char* kRegistryKey_AudioBackend   = "Caesura.AudioBackend";
static const char* kRegistryKey_PlatformBackend = "Caesura.PlatformBackend";
static const char* kRegistryKey_InputRouter    = "Caesura.InputRouter";

// -- Lua C functions -------------------------------------------------------

static int lua_Engine_select_render_backend(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    const char* subBackend = luaL_optstring(L, 2, nullptr);

    auto& registry = BackendRegistry::instance();
    IRenderDevice* device = registry.createRenderDevice(name);
    if (!device) {
        lua_pushnil(L);
        lua_pushfstring(L, "Unknown render backend: %s", name);
        return 2;
    }

    lua_pushlightuserdata(L, device);
    lua_setfield(L, LUA_REGISTRYINDEX, kRegistryKey_RenderDevice);

    if (subBackend && strcmp(name, "bgfx") == 0) {
        BgfxRenderDevice* bgfxDev = dynamic_cast<BgfxRenderDevice*>(device);
        if (bgfxDev) {
            bgfxDev->setPreferredBackend(subBackend);
        }
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_Engine_select_audio_backend(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    auto& registry = BackendRegistry::instance();
    IAudioBackend* backend = registry.createAudioBackend(name);
    if (!backend) {
        lua_pushnil(L);
        lua_pushfstring(L, "Unknown audio backend: %s", name);
        return 2;
    }

    lua_pushlightuserdata(L, backend);
    lua_setfield(L, LUA_REGISTRYINDEX, kRegistryKey_AudioBackend);

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_Engine_select_platform_backend(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    auto& registry = BackendRegistry::instance();
    IPlatformBackend* backend = registry.createPlatformBackend(name);
    if (!backend) {
        lua_pushnil(L);
        lua_pushfstring(L, "Unknown platform backend: %s", name);
        return 2;
    }

    lua_pushlightuserdata(L, backend);
    lua_setfield(L, LUA_REGISTRYINDEX, kRegistryKey_PlatformBackend);

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_Engine_get_backend_info(lua_State* L) {
    auto& registry = BackendRegistry::instance();

    lua_newtable(L);

    IRenderDevice* renderDev = registry.getRenderDevice();
    if (renderDev) {
        lua_pushstring(L, "bgfx");
        lua_setfield(L, -2, "render");
    }

    IAudioBackend* audioBackend = registry.getAudioBackend();
    if (audioBackend) {
        lua_pushstring(L, audioBackend->getBackendName());
        lua_setfield(L, -2, "audio");
    }

    IPlatformBackend* platBackend = registry.getPlatformBackend();
    if (platBackend) {
        lua_pushstring(L, platBackend->getBackendName());
        lua_setfield(L, -2, "platform");
    }

    return 1;
}

// -- Registry helpers: resolve pointers from Lua state ---------------------

IRenderDevice* BackendRegistry::getRenderDeviceFromLua(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, kRegistryKey_RenderDevice);
    IRenderDevice* dev = static_cast<IRenderDevice*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!dev) dev = BackendRegistry::instance().getRenderDevice();
    return dev;
}

IAudioBackend* BackendRegistry::getAudioBackendFromLua(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, kRegistryKey_AudioBackend);
    IAudioBackend* backend = static_cast<IAudioBackend*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!backend) backend = BackendRegistry::instance().getAudioBackend();
    return backend;
}

IPlatformBackend* BackendRegistry::getPlatformBackendFromLua(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, kRegistryKey_PlatformBackend);
    IPlatformBackend* backend = static_cast<IPlatformBackend*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!backend) backend = BackendRegistry::instance().getPlatformBackend();
    return backend;
}

InputRouter* BackendRegistry::getInputRouterFromLua(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, kRegistryKey_InputRouter);
    InputRouter* router = static_cast<InputRouter*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!router) router = BackendRegistry::instance().getInputRouter();
    return router;
}

VideoPlayer* BackendRegistry::getVideoPlayerFromLua(lua_State* L) {
    return BackendRegistry::instance().getVideoPlayer();
}

// -- Register Engine.* bindings to Lua -------------------------------------

void BackendRegistry::registerEngineBindings(lua_State* L) {
    static const luaL_Reg engine_funcs[] = {
        { "select_render_backend",   lua_Engine_select_render_backend   },
        { "select_audio_backend",    lua_Engine_select_audio_backend    },
        { "select_platform_backend", lua_Engine_select_platform_backend },
        { "get_backend_info",        lua_Engine_get_backend_info        },
        { nullptr, nullptr }
    };

    luaL_newlib(L, engine_funcs);
    lua_setglobal(L, "Engine");
    printf("[Lua] Engine (backend selection) module registered.\n");
}

} // namespace Caesura

