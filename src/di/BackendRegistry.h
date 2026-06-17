#pragma once
#include "../resource/ResourceHandle.h"
#include <typeindex>
#include <unordered_map>
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
class IRpcServer;
class IEditorServer;
class ISandboxQuota;
class ITextureBudget;
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
    void setRenderDevice(IRenderDevice& device);
    void setAudioBackend(IAudioBackend& backend);
    void setPlatformBackend(IPlatformBackend& backend);
    void setInputRouter(IInputRouter* router);
    void setMiniGameBackend(IMiniGameBackend* backend);
    void setAnimationBackend(IAnimationBackend* be);
    void setCryptoEngine(carc::ICryptoEngine* engine);
    void setLuaManager(ILuaManager* mgr);
    void setJobSystem(IJobSystem* js);
    void setRpcServer(IRpcServer* rpc);
    void setEditorServer(IEditorServer* es);
    void setSandboxQuota(ISandboxQuota* sq);
    void setVideoPlayer(IVideoPlayer* player);
    void setTextureManager(ITextureManager* mgr);
    void setParticleSystem(IParticleSystem* ps);
    void setDebugManager(IDebugManager* dm);
    void setAsyncLoader(IAsyncLoader* al);
    void setLayerManager(ILayerManager* mgr);
    void setTextureBudget(ITextureBudget* tb);

    void setLuaState(lua_State* L) { m_luaState = L; }
    lua_State* getLuaState() { return m_luaState; }

    // -- SandboxQuota wrappers (out-of-line — need SandboxQuota type) --
    bool tryAlloc(const char* kind);
    void release(const char* kind);

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
    IRpcServer*       getRpcServer();
    IEditorServer*    getEditorServer();
    ISandboxQuota*    getSandboxQuota();
    ITextureBudget*   getTextureBudget();

    void registerNullBackends();

    // -- Factories --
    IAudioBackend*    createAudioBackend(const char* name);
    IRenderDevice*    createRenderDevice(const char* name);
    IPlatformBackend* createPlatformBackend(const char* name);

    // -- Lua --
    static void registerEngineBindings(lua_State* L);
    static IRenderDevice*    getRenderDeviceFromLua(lua_State* L);
    static IAudioBackend*    getAudioBackendFromLua(lua_State* L);
    static IPlatformBackend* getPlatformBackendFromLua(lua_State* L);
    static IInputRouter*     getInputRouterFromLua(lua_State* L);
    static IMiniGameBackend* getMiniGameBackendFromLua(lua_State* L);
    static IVideoPlayer*     getVideoPlayerFromLua(lua_State* L);

    // -- ResourceHandle / Generation tracking --
    GenerationTracker& generations() { return m_generations; }
    bool isValidHandle(const ResourceHandle& h) const { return h.id != 0 && m_generations.isCurrent(h); }
    void invalidateHandles(HandleType type) { m_generations.invalidate(type); }
    ResourceHandle makeHandle(HandleType type, uint32_t id) { return m_generations.makeHandle(type, id); }

private:
    BackendRegistry() = default;
    std::unordered_map<std::type_index, void*> m_services;
    lua_State*         m_luaState    = nullptr;
    GenerationTracker  m_generations;
};

} // namespace Caesura
