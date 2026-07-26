extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "BackendRegistry.h"
// All interface includes needed for getService<I>() / setService<I>() template instantiation
#include "../audio/api/IAudioBackend.h"
#include "../platform/api/IPlatformBackend.h"
#include "../render/api/IRenderDevice.h"
#include "../render/api/IVideoPlayer.h"
#include "../render/api/ITextureManager.h"
#include "../render/api/IParticleSystem.h"
#include "../render/api/ILayerManager.h"
#include "../debug/api/IDebugManager.h"
#include "../resource/api/IAsyncLoader.h"
#include "../resource/api/IResourceGenerationTracker.h"
#include "../minigame/api/IMiniGameBackend.h"
#include "../live2d/api/IAnimationBackend.h"
#include "../archive/api/ICryptoEngine.h"
#include "../script/api/ILuaManager.h"
#include "../job/api/IJobSystem.h"
#include "api/ISandboxQuota.h"
#include "../input/api/IInputRouter.h"
#include "../di/api/ITextureBudget.h"
#include "../storage/api/ISaveManager.h"
#include "../steam/api/ISteamBackend.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace Caesura {

// -- Singleton -------------------------------------------------------------

BackendRegistry& BackendRegistry::instance() {
    static BackendRegistry inst;
    return inst;
}

// -- SandboxQuota wrappers --------------------------------------------------

bool BackendRegistry::tryAlloc(const char* kind) {
    auto* quota = getSandboxQuota();
    return quota ? quota->tryAlloc(kind) : true;
}

void BackendRegistry::release(const char* kind) {
    if (auto* quota = getSandboxQuota()) quota->release(kind);
}

// -- Getter / Setter definitions (need complete types) --------------------

#define DEF_GETTER(Iface, method) \
    Iface* BackendRegistry::get##method() { return getService<Iface>(); }

#define DEF_SETTER(Iface, method) \
    void BackendRegistry::set##method(Iface* p) { setService(p); }

DEF_GETTER(IRenderDevice,    RenderDevice)
DEF_GETTER(IAudioBackend,    AudioBackend)
DEF_GETTER(IPlatformBackend, PlatformBackend)
DEF_GETTER(IInputRouter,     InputRouter)
DEF_GETTER(IVideoPlayer,     VideoPlayer)
DEF_GETTER(ITextureManager,  TextureManager)
DEF_GETTER(ILayerManager,    LayerManager)
DEF_GETTER(IParticleSystem,  ParticleSystem)
DEF_GETTER(IDebugManager,    DebugManager)
DEF_GETTER(IAsyncLoader,     AsyncLoader)
DEF_GETTER(IMiniGameBackend, MiniGameBackend)
DEF_GETTER(IAnimationBackend, AnimationBackend)
DEF_GETTER(carc::ICryptoEngine, CryptoEngine)
DEF_GETTER(ILuaManager,      LuaManager)
DEF_GETTER(IJobSystem,       JobSystem)
DEF_GETTER(ISandboxQuota,    SandboxQuota)
DEF_GETTER(ITextureBudget,   TextureBudget)
DEF_GETTER(ISaveManager,     SaveManager)
DEF_GETTER(IResourceGenerationTracker, ResourceGenerationTracker)
DEF_GETTER(ISteamBackend,    SteamBackend)

DEF_SETTER(IRenderDevice,    RenderDevice)
DEF_SETTER(IAudioBackend,    AudioBackend)
DEF_SETTER(IPlatformBackend, PlatformBackend)
DEF_SETTER(IInputRouter,     InputRouter)
DEF_SETTER(IMiniGameBackend, MiniGameBackend)
DEF_SETTER(IAnimationBackend, AnimationBackend)
DEF_SETTER(carc::ICryptoEngine, CryptoEngine)
DEF_SETTER(ILuaManager,      LuaManager)
DEF_SETTER(IJobSystem,       JobSystem)
DEF_SETTER(ISandboxQuota,    SandboxQuota)
DEF_SETTER(IVideoPlayer,     VideoPlayer)
DEF_SETTER(ITextureManager,  TextureManager)
DEF_SETTER(IParticleSystem,  ParticleSystem)
DEF_SETTER(IDebugManager,    DebugManager)
DEF_SETTER(IAsyncLoader,     AsyncLoader)
DEF_SETTER(ILayerManager,    LayerManager)
DEF_SETTER(ITextureBudget,   TextureBudget)
DEF_SETTER(ISaveManager,     SaveManager)
DEF_SETTER(IResourceGenerationTracker, ResourceGenerationTracker)
DEF_SETTER(ISteamBackend,    SteamBackend)

#undef DEF_GETTER
#undef DEF_SETTER

// -- Device loss recovery listeners ---------------------------------------

void BackendRegistry::registerDeviceLostListener(IDeviceLostListener* listener) {
    if (listener) m_deviceLostListeners.push_back(listener);
}

void BackendRegistry::unregisterDeviceLostListener(IDeviceLostListener* listener) {
    auto it = std::find(m_deviceLostListeners.begin(), m_deviceLostListeners.end(), listener);
    if (it != m_deviceLostListeners.end()) m_deviceLostListeners.erase(it);
}

void BackendRegistry::notifyDeviceLost() {
    printf("[BackendRegistry] Notifying %zu listeners: onDeviceLost\n", m_deviceLostListeners.size());
    for (auto* listener : m_deviceLostListeners) {
        listener->onDeviceLost();
    }
}

void BackendRegistry::notifyDeviceRestored() {
    printf("[BackendRegistry] Notifying %zu listeners: onDeviceRestored\n", m_deviceLostListeners.size());
    for (auto* listener : m_deviceLostListeners) {
        listener->onDeviceRestored();
    }
}

// -- Backend factory -------------------------------------------------------

IAudioBackend* BackendRegistry::createAudioBackend(const char* name) {
    // Factory only returns pre-registered backend.
    // Engine owns lifecycle �� use setAudioBackend() to register first.
    if (strcmp(name, "soloud") == 0 || strcmp(name, "SoLoud") == 0) {
        if (getService<IAudioBackend>()) {
            printf("[BackendRegistry] Using pre-registered audio backend: SoLoud\n");
            return getService<IAudioBackend>();
        }
        fprintf(stderr, "[BackendRegistry] SoLoud backend not registered yet\n");
        return nullptr;
    }
    if (strcmp(name, "null") == 0 || strcmp(name, "Null") == 0) {
        if (getService<IAudioBackend>()) return getService<IAudioBackend>();
        fprintf(stderr, "[BackendRegistry] Null audio backend not registered yet\n");
        return nullptr;
    }
    fprintf(stderr, "[BackendRegistry] Unknown audio backend: %s\n", name);
    return nullptr;
}

IRenderDevice* BackendRegistry::createRenderDevice(const char* name) {
    // Factory only returns pre-registered backend.
    // Engine owns lifecycle �� use setRenderDevice() to register first.
    if (strcmp(name, "bgfx") == 0) {
        if (getService<IRenderDevice>()) {
            printf("[BackendRegistry] Using pre-registered render backend: bgfx\n");
            return getService<IRenderDevice>();
        }
        fprintf(stderr, "[BackendRegistry] bgfx backend not registered yet\n");
        return nullptr;
    }
    if (strcmp(name, "null") == 0 || strcmp(name, "Null") == 0) {
        if (getService<IRenderDevice>()) return getService<IRenderDevice>();
        fprintf(stderr, "[BackendRegistry] Null render backend not registered yet\n");
        return nullptr;
    }
    fprintf(stderr, "[BackendRegistry] Unknown render backend: %s\n", name);
    return nullptr;
}

IPlatformBackend* BackendRegistry::createPlatformBackend(const char* name) {
    // Factory only returns pre-registered backend.
    // Engine owns lifecycle �� use setPlatformBackend() to register first.
    if (strcmp(name, "sdl3") == 0 || strcmp(name, "SDL3") == 0) {
        if (getService<IPlatformBackend>()) {
            printf("[BackendRegistry] Using pre-registered platform backend: SDL3\n");
            return getService<IPlatformBackend>();
        }
        fprintf(stderr, "[BackendRegistry] SDL3 backend not registered yet\n");
        return nullptr;
    }
    if (strcmp(name, "null") == 0 || strcmp(name, "Null") == 0) {
        if (getService<IPlatformBackend>()) return getService<IPlatformBackend>();
        fprintf(stderr, "[BackendRegistry] Null platform backend not registered yet\n");
        return nullptr;
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

    if (subBackend) {
        device->setPreferredBackend(subBackend);
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
        lua_pushstring(L, renderDev->getBackendName());
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

IInputRouter* BackendRegistry::getInputRouterFromLua(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, kRegistryKey_InputRouter);
    IInputRouter* router = static_cast<IInputRouter*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!router) router = BackendRegistry::instance().getInputRouter();
    return router;
}

IVideoPlayer* BackendRegistry::getVideoPlayerFromLua(lua_State* L) {  
    (void)L;  // singleton fallback — see getMiniGameBackendFromLua
    return BackendRegistry::instance().getVideoPlayer();
}

// -- Register Engine.* bindings to Lua -------------------------------------

IMiniGameBackend* BackendRegistry::getMiniGameBackendFromLua(lua_State* L) {
    return BackendRegistry::instance().getMiniGameBackend();
}

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
