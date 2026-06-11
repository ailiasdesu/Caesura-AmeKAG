#pragma once
#include "../Audio/api/IAudioBackend.h"
#include "../platform/api/IPlatformBackend.h"
#include "../Render/IRenderDevice.h"
#include "../Resource/ResourceHandle.h"
#include "../Render/VideoPlayer.h"
#include "../Render/TextureManager.h"
#include "../Render/ParticleSystem.h"
#include "../Debug/DebugManager.h"
#include "../Resource/AsyncLoader.h"
#include "../MiniGame/IMiniGameBackend.h"
#include "../Live2D/IAnimationBackend.h"
#include "../input/InputRouter.h"
#include <unordered_map>
#include <string>
#include <memory>

struct lua_State;

namespace Caesura {

// Forward declarations for render subsystem managers
class TextureBudget;
class TextureManager;
class LayerManager;
class IMiniGameBackend;

// ---------------------------------------------------------------------------
// BackendRegistry -- Singleton factory/registry for all engine backends
// ---------------------------------------------------------------------------
// Manages:
//   - IRenderDevice (bgfx, etc.)
//   - IAudioBackend (SoLoud, etc.)
//   - IPlatformBackend (SDL3, etc.)
//   - VideoPlayer (MPEG1 playback)
//   - InputRouter (shared across all backends)
//   - TextureManager (texture lifecycle)
//   - LayerManager (bg/fg/message layer state)
//
// Lua accesses backends through the registry, not through global C++ pointers.
// At init time, Lua calls Engine.select_backend("soloud") etc. to configure.
// All KAG/Render/DevCore bindings then look up the active backend from here.

class BackendRegistry {
public:
    static BackendRegistry& instance();

    BackendRegistry(const BackendRegistry&) = delete;
    BackendRegistry& operator=(const BackendRegistry&) = delete;

    // -- Set active backends (called by Engine during init or Lua config) --
    void setRenderDevice(IRenderDevice& device);
    void setAudioBackend(IAudioBackend& backend);
    void setPlatformBackend(IPlatformBackend& backend);
    void setInputRouter(InputRouter* router);
    void setMiniGameBackend(IMiniGameBackend* backend);
    IMiniGameBackend* getMiniGameBackend() { return m_miniGameBackend; }

    void setAnimationBackend(IAnimationBackend* backend) { m_animationBackend = backend; }
    IAnimationBackend* getAnimationBackend() { return m_animationBackend; }

    void setVideoPlayer(VideoPlayer* player);

    // Install null (no-op) render and platform backends for headless mode
    void registerNullBackends();
    void setTextureManager(TextureManager* mgr);
    void setParticleSystem(ParticleSystem* ps) { m_particleSystem = ps; }
    ParticleSystem* getParticleSystem() { return m_particleSystem; }
    void setDebugManager(DebugManager* dm) { m_debugManager = dm; }
    DebugManager* getDebugManager() { return m_debugManager; }
    void setAsyncLoader(AsyncLoader* al) { m_asyncLoader = al; }
    AsyncLoader* getAsyncLoader() { return m_asyncLoader; }
    void setLayerManager(LayerManager* mgr);

    // -- Get active backends -----------------------------------------------
    IRenderDevice*   getRenderDevice()   { return m_renderDevice; }  // non-owning; owned by Engine
    IAudioBackend*   getAudioBackend()   { return m_audioBackend; }  // non-owning; owned by Engine
    IPlatformBackend* getPlatformBackend() { return m_platformBackend; }  // non-owning; owned by Engine
    InputRouter*     getInputRouter()    { return m_inputRouter; }
    VideoPlayer*     getVideoPlayer()    { return m_videoPlayer; }
    TextureManager*  getTextureManager() { return m_textureManager; }
    LayerManager*    getLayerManager()   { return m_layerManager; }
    TextureBudget*  getTextureBudget()  { return m_textureBudget; }
    void setTextureBudget(TextureBudget* tb) { m_textureBudget = tb; }

    // -- Backend lookup: resolve by name (no allocation) --------------------
    // Returns existing backend pointer (Engine owns lifecycle via unique_ptr).
    IAudioBackend*   createAudioBackend(const char* name);
    IRenderDevice*   createRenderDevice(const char* name);
    IPlatformBackend* createPlatformBackend(const char* name);

    // -- Lua binding: expose as Engine table functions ---------------------
    static void registerEngineBindings(lua_State* L);

    // -- Lua C function helpers --------------------------------------------
    static IRenderDevice*   getRenderDeviceFromLua(lua_State* L);
    static IAudioBackend*   getAudioBackendFromLua(lua_State* L);
    static IPlatformBackend* getPlatformBackendFromLua(lua_State* L);
    static InputRouter*     getInputRouterFromLua(lua_State* L);
    static IMiniGameBackend* getMiniGameBackendFromLua(lua_State* L);
    static VideoPlayer*     getVideoPlayerFromLua(lua_State* L);

    // -- ResourceHandle / Generation tracking -----------------------------------
    GenerationTracker& generations() { return m_generations; }

    // Validate a handle: checks id != 0 AND generation matches current
    bool isValidHandle(const ResourceHandle& h) const {
        return h.id != 0 && m_generations.isCurrent(h);
    }

    // Invalidate all handles of a type (call on hot reload)
    void invalidateHandles(HandleType type) {
        m_generations.invalidate(type);
    }

    // Create a handle stamped with the current generation
    ResourceHandle makeHandle(HandleType type, uint32_t id) {
        return m_generations.makeHandle(type, id);
    }

private:
    BackendRegistry() = default;

    IRenderDevice*   m_renderDevice    = nullptr;   // non-owning; Engine holds unique_ptr
        IAudioBackend*   m_audioBackend    = nullptr;   // non-owning; Engine holds unique_ptr
        IPlatformBackend* m_platformBackend = nullptr;  // non-owning; Engine holds unique_ptr
        InputRouter*     m_inputRouter     = nullptr;
    GenerationTracker m_generations;
    IMiniGameBackend* m_miniGameBackend = nullptr;
    IAnimationBackend* m_animationBackend = nullptr;
    VideoPlayer*     m_videoPlayer     = nullptr;
    ParticleSystem*  m_particleSystem  = nullptr;
    DebugManager*    m_debugManager    = nullptr;
    AsyncLoader*     m_asyncLoader     = nullptr;
    TextureManager*  m_textureManager  = nullptr;
    LayerManager*    m_layerManager    = nullptr;
    TextureBudget*  m_textureBudget  = nullptr;
};

} // namespace Caesura
