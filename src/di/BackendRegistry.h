#pragma once
#include "../audio/api/IAudioBackend.h"
#include "../platform/api/IPlatformBackend.h"
#include "../render/IRenderDevice.h"
#include "../resource/ResourceHandle.h"
#include "../render/api/IVideoPlayer.h"
#include "../render/api/ITextureManager.h"
#include "../render/api/IParticleSystem.h"
#include "../debug/api/IDebugManager.h"
#include "../resource/api/IAsyncLoader.h"
#include "../minigame/api/IMiniGameBackend.h"
#include "../live2d/IAnimationBackend.h"
#include "../input/api/IInputRouter.h"
#include "../di/api/ITextureBudget.h"
#include "../render/api/ILayerManager.h"
#include <unordered_map>
#include <string>
#include <memory>

struct lua_State;

namespace Caesura {

// ============================================================================
// BackendRegistry -- Singleton factory/registry for all engine backends
// ============================================================================
// All backends are stored as interface pointers. Concrete implementations
// are created by main.cpp / Engine::init() and registered here.

class BackendRegistry {
public:
    static BackendRegistry& instance();

    BackendRegistry(const BackendRegistry&) = delete;
    BackendRegistry& operator=(const BackendRegistry&) = delete;

    // -- Set active backends (called by Engine during init or Lua config) --
    void setRenderDevice(IRenderDevice& device);
    void setAudioBackend(IAudioBackend& backend);
    void setPlatformBackend(IPlatformBackend& backend);
    void setInputRouter(IInputRouter* router);
    void setMiniGameBackend(IMiniGameBackend* backend);
    IMiniGameBackend* getMiniGameBackend() { return m_miniGameBackend; }

    void setAnimationBackend(IAnimationBackend* backend) { m_animationBackend = backend; }
    IAnimationBackend* getAnimationBackend() { return m_animationBackend; }

    void setVideoPlayer(IVideoPlayer* player);

    void registerNullBackends();
    void setTextureManager(ITextureManager* mgr);
    void setParticleSystem(IParticleSystem* ps) { m_particleSystem = ps; }
    IParticleSystem* getParticleSystem() { return m_particleSystem; }
    void setDebugManager(IDebugManager* dm) { m_debugManager = dm; }
    IDebugManager* getDebugManager() { return m_debugManager; }
    void setAsyncLoader(IAsyncLoader* al) { m_asyncLoader = al; }
    IAsyncLoader* getAsyncLoader() { return m_asyncLoader; }
    void setLayerManager(ILayerManager* mgr);

    // -- Get active backends -----------------------------------------------
    IRenderDevice*    getRenderDevice()    { return m_renderDevice; }
    IAudioBackend*    getAudioBackend()    { return m_audioBackend; }
    IPlatformBackend* getPlatformBackend() { return m_platformBackend; }
    IInputRouter*     getInputRouter()     { return m_inputRouter; }
    IVideoPlayer*     getVideoPlayer()     { return m_videoPlayer; }
    ITextureManager*  getTextureManager()  { return m_textureManager; }
    ILayerManager*    getLayerManager()    { return m_layerManager; }
    ITextureBudget*   getTextureBudget()   { return m_textureBudget; }
    void setTextureBudget(ITextureBudget* tb) { m_textureBudget = tb; }

    // -- Backend lookup: resolve by name (no allocation) --------------------
    IAudioBackend*    createAudioBackend(const char* name);
    IRenderDevice*    createRenderDevice(const char* name);
    IPlatformBackend* createPlatformBackend(const char* name);

    // -- Lua binding: expose as Engine table functions ---------------------
    static void registerEngineBindings(lua_State* L);

    // -- Lua C function helpers --------------------------------------------
    static IRenderDevice*    getRenderDeviceFromLua(lua_State* L);
    static IAudioBackend*    getAudioBackendFromLua(lua_State* L);
    static IPlatformBackend* getPlatformBackendFromLua(lua_State* L);
    static IInputRouter*     getInputRouterFromLua(lua_State* L);
    static IMiniGameBackend* getMiniGameBackendFromLua(lua_State* L);
    static IVideoPlayer*     getVideoPlayerFromLua(lua_State* L);

    // -- ResourceHandle / Generation tracking -----------------------------------
    GenerationTracker& generations() { return m_generations; }

    bool isValidHandle(const ResourceHandle& h) const {
        return h.id != 0 && m_generations.isCurrent(h);
    }

    void invalidateHandles(HandleType type) {
        m_generations.invalidate(type);
    }

    ResourceHandle makeHandle(HandleType type, uint32_t id) {
        return m_generations.makeHandle(type, id);
    }

private:
    BackendRegistry() = default;

    IRenderDevice*    m_renderDevice    = nullptr;
    IAudioBackend*    m_audioBackend    = nullptr;
    IPlatformBackend* m_platformBackend = nullptr;
    IInputRouter*     m_inputRouter     = nullptr;
    GenerationTracker m_generations;
    IMiniGameBackend*  m_miniGameBackend  = nullptr;
    IAnimationBackend* m_animationBackend = nullptr;
    IVideoPlayer*      m_videoPlayer      = nullptr;
    IParticleSystem*   m_particleSystem   = nullptr;
    IDebugManager*     m_debugManager     = nullptr;
    IAsyncLoader*      m_asyncLoader      = nullptr;
    ITextureManager*   m_textureManager   = nullptr;
    ILayerManager*     m_layerManager     = nullptr;
    ITextureBudget*    m_textureBudget    = nullptr;
};

} // namespace Caesura