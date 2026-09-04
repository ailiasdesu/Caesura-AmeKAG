#include "BackendRegistry.h"
// All interface includes needed for getService<I>() / setService<I>() template instantiation
#include "../audio/api/IAudioBackend.h"
#include "../platform/api/IPlatformBackend.h"
#include "../render/api/IRenderDevice.h"
#include "../render/api/IVideoPlayer.h"
#include "../render/api/ITextureManager.h"
#include "../render/api/IParticleSystem.h"
#include "../render/api/ILayerManager.h"
#include "../render/api/IMeshRenderer.h"
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
#include "../platform/api/IMobileAdapter.h"
#include "../platform/api/IDisplayService.h"
#include "../platform/api/ILifecycleService.h"
#include "../audio/api/IAudioFocusService.h"
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

int BackendRegistry::count(const char* kind) {
    auto* quota = getSandboxQuota();
    return quota ? quota->count(kind) : 0;
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
DEF_GETTER(IMobileAdapter,   MobileAdapter)
DEF_GETTER(IDisplayService,  DisplayService)
DEF_GETTER(ILifecycleService, LifecycleService)
DEF_GETTER(IAudioFocusService, AudioFocusService)
DEF_GETTER(IMeshRenderer,    MeshRenderer)

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
DEF_SETTER(IMobileAdapter,   MobileAdapter)
DEF_SETTER(IDisplayService,  DisplayService)
DEF_SETTER(ILifecycleService, LifecycleService)
DEF_SETTER(IAudioFocusService, AudioFocusService)
DEF_SETTER(IMeshRenderer,    MeshRenderer)

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
    const auto listeners = m_deviceLostListeners;  // copy: listeners may unregister during the callback
    for (auto* listener : listeners) {
        listener->onDeviceLost();
    }
}

void BackendRegistry::notifyDeviceRestored() {
    printf("[BackendRegistry] Notifying %zu listeners: onDeviceRestored\n", m_deviceLostListeners.size());
    const auto listeners = m_deviceLostListeners;  // copy: listeners may unregister during the callback
    for (auto* listener : listeners) {
        listener->onDeviceRestored();
    }
}

// -- Backend factory -------------------------------------------------------

IAudioBackend* BackendRegistry::createAudioBackend(const char* name) {
    // Factory only returns pre-registered backend.
    // Engine owns lifecycle →→ use setAudioBackend() to register first.
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
    // Engine owns lifecycle →→ use setRenderDevice() to register first.
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
    // Engine owns lifecycle →→ use setPlatformBackend() to register first.
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

void BackendRegistry::setErrorReporter(ErrorReporter reporter) {
    m_errorReporter = std::move(reporter);
}

const BackendRegistry::ErrorReporter& BackendRegistry::getErrorReporter() const {
    return m_errorReporter;
}

// (Round 21 P1-5) The Lua "Engine" binding and registry-key helpers moved
// to script/bindings/EngineBinding.*; the DI container no longer depends on
// the Lua C API.
} // namespace Caesura