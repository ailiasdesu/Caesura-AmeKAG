#pragma once
#include "IAudioBackend.h"
#include "IPlatformBackend.h"
#include "../Render/IRenderDevice.h"
#include "ResourceHandle.h"
#include "../Render/VideoPlayer.h"
#include "InputRouter.h"
#include <unordered_map>
#include <string>
#include <memory>

struct lua_State;

namespace Caesura {

// Forward declarations for render subsystem managers
class TextureManager;
class LayerManager;

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
    void setRenderDevice(IRenderDevice* device);
    void setAudioBackend(IAudioBackend* backend);
    void setPlatformBackend(IPlatformBackend* backend);
    void setInputRouter(InputRouter* router);
    void setVideoPlayer(VideoPlayer* player);
    void setTextureManager(TextureManager* mgr);
    void setLayerManager(LayerManager* mgr);

    // -- Get active backends -----------------------------------------------
    IRenderDevice*   getRenderDevice()   { return m_renderDevice; }
    IAudioBackend*   getAudioBackend()   { return m_audioBackend; }
    IPlatformBackend* getPlatformBackend() { return m_platformBackend; }
    InputRouter*     getInputRouter()    { return m_inputRouter; }
    VideoPlayer*     getVideoPlayer()    { return m_videoPlayer; }
    TextureManager*  getTextureManager() { return m_textureManager; }
    LayerManager*    getLayerManager()   { return m_layerManager; }

    // -- Backend factory: create by name -----------------------------------
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

    IRenderDevice*   m_renderDevice    = nullptr;
    std::unique_ptr<IRenderDevice>     m_ownedRenderDevice;
    IAudioBackend*   m_audioBackend    = nullptr;
    std::unique_ptr<IAudioBackend>     m_ownedAudioBackend;
    IPlatformBackend* m_platformBackend = nullptr;
    std::unique_ptr<IPlatformBackend>  m_ownedPlatformBackend;
    InputRouter*     m_inputRouter     = nullptr;
    GenerationTracker m_generations;
    VideoPlayer*     m_videoPlayer     = nullptr;
    TextureManager*  m_textureManager  = nullptr;
    LayerManager*    m_layerManager    = nullptr;
};

} // namespace Caesura
