#pragma once
#include "api/IDeviceLostListener.h"
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <string>

struct lua_State;

namespace Caesura {

// Forward declarations only — no I*.h includes needed for consumers
class IRenderDevice;
class IAudioBackend;
class IPlatformBackend;
class IInputRouter;
class IVideoPlayer;
class ITextureManager;
class ILayerManager;
class IParticleSystem;
class IDebugManager;
class IAsyncLoader;
class IMiniGameBackend;
class IAnimationBackend;
class ILuaManager;
class IJobSystem;
class ISandboxQuota;
class ITextureBudget;
class ISaveManager;
class IResourceGenerationTracker;
class ISteamBackend;
class IMobileAdapter;
class IMeshRenderer;
namespace carc { class ICryptoEngine; }

class BackendRegistry {
public:
    static BackendRegistry& instance();
    BackendRegistry(const BackendRegistry&) = delete;
    BackendRegistry& operator=(const BackendRegistry&) = delete;

    // -- Type-erased storage (header-only for template, needs complete type at call site)
    template<typename I>
    void setService(I* impl) { m_services[std::type_index(typeid(I))] = static_cast<void*>(impl); }

    template<typename I>
    I* getService() const {
        auto it = m_services.find(std::type_index(typeid(I)));
        return (it != m_services.end()) ? static_cast<I*>(it->second) : nullptr;
    }

    // -- Setters (out-of-line — need complete types in .cpp) --
    void setRenderDevice(IRenderDevice* device);
    void setAudioBackend(IAudioBackend* backend);
    void setPlatformBackend(IPlatformBackend* backend);
    void setInputRouter(IInputRouter* router);
    void setMiniGameBackend(IMiniGameBackend* backend);
    void setAnimationBackend(IAnimationBackend* be);
    void setCryptoEngine(carc::ICryptoEngine* engine);
    void setLuaManager(ILuaManager* mgr);
    void setJobSystem(IJobSystem* js);
    void setSandboxQuota(ISandboxQuota* sq);
    void setVideoPlayer(IVideoPlayer* player);
    void setTextureManager(ITextureManager* mgr);
    void setParticleSystem(IParticleSystem* ps);
    void setDebugManager(IDebugManager* dm);
    void setAsyncLoader(IAsyncLoader* al);
    void setLayerManager(ILayerManager* mgr);
    void setTextureBudget(ITextureBudget* tb);
    void setSaveManager(ISaveManager* manager);
    void setResourceGenerationTracker(IResourceGenerationTracker* tracker);
    void setSteamBackend(ISteamBackend* backend);
    void setMobileAdapter(IMobileAdapter* adapter);
    void setMeshRenderer(IMeshRenderer* renderer);

    void setLuaState(lua_State* L) { m_luaState = L; }
    lua_State* getLuaState() { return m_luaState; }

    // -- SandboxQuota wrappers (delegate to the registered interface) --
    bool tryAlloc(const char* kind);
    void release(const char* kind);
    // Current sandbox-quota count for `kind` (0 when no quota is installed).
    int count(const char* kind);

    // -- Getters (out-of-line — need complete types in .cpp) --
    IRenderDevice*    getRenderDevice();
    IAudioBackend*    getAudioBackend();
    IPlatformBackend* getPlatformBackend();
    IInputRouter*     getInputRouter();
    IVideoPlayer*     getVideoPlayer();
    ITextureManager*  getTextureManager();
    ILayerManager*    getLayerManager();
    IParticleSystem*  getParticleSystem();
    IDebugManager*    getDebugManager();
    IAsyncLoader*     getAsyncLoader();
    IMiniGameBackend* getMiniGameBackend();
    IAnimationBackend* getAnimationBackend();
    carc::ICryptoEngine* getCryptoEngine();
    ILuaManager*      getLuaManager();
    IJobSystem*       getJobSystem();
    ISandboxQuota*    getSandboxQuota();
    ITextureBudget*   getTextureBudget();
    ISaveManager*     getSaveManager();
    IResourceGenerationTracker* getResourceGenerationTracker();
    ISteamBackend*    getSteamBackend();
    IMobileAdapter*   getMobileAdapter();
    IMeshRenderer*    getMeshRenderer();

    // -- Factories --
    IAudioBackend*    createAudioBackend(const char* name);
    IRenderDevice*    createRenderDevice(const char* name);
    IPlatformBackend* createPlatformBackend(const char* name);

    // -- Device loss recovery listeners --
    void registerDeviceLostListener(IDeviceLostListener* listener);
    void unregisterDeviceLostListener(IDeviceLostListener* listener);
    void notifyDeviceLost();
    void notifyDeviceRestored();

    // -- Lua --
    static void registerEngineBindings(lua_State* L);
    static IRenderDevice*    getRenderDeviceFromLua(lua_State* L);
    static IAudioBackend*    getAudioBackendFromLua(lua_State* L);
    static IPlatformBackend* getPlatformBackendFromLua(lua_State* L);
    static IInputRouter*     getInputRouterFromLua(lua_State* L);
    static IMiniGameBackend* getMiniGameBackendFromLua(lua_State* L);
    static IVideoPlayer*     getVideoPlayerFromLua(lua_State* L);

private:
    BackendRegistry() = default;
    std::unordered_map<std::type_index, void*> m_services;
    std::vector<IDeviceLostListener*> m_deviceLostListeners;
    lua_State*         m_luaState    = nullptr;
};

} // namespace Caesura
