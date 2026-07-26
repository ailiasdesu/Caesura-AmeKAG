extern "C" {
#include <lua.h>
}

#include "../debug/DebugManager.h"
#include "../debug/api/IDebugManager.h"
#include "../di/BackendRegistry.h"
#include "../input/api/IInputRouter.h"
#include "../minigame/api/IMiniGameBackend.h"
#include "../render/api/ITextureManager.h"
#include "../render/api/IVideoPlayer.h"
#include "../render/api/IParticleSystem.h"
#include "../resource/api/IAsyncLoader.h"

namespace Caesura {

namespace {

void setRegistryLightUserData(lua_State* L, const char* key, void* value) {
    lua_pushlightuserdata(L, value);
    lua_setfield(L, LUA_REGISTRYINDEX, key);
}

} // namespace

void registerEngineLuaRegistryServices(lua_State* L,
                                       IInputRouter* inputRouter,
                                       IVideoPlayer* videoPlayer,
                                       IParticleSystem* particleSystem,
                                       ITextureManager* textureManager,
                                       IAsyncLoader* asyncLoader) {
    if (!L) return;

    auto& registry = BackendRegistry::instance();
    setRegistryLightUserData(L, "Caesura.RenderDevice", registry.getRenderDevice());
    setRegistryLightUserData(L, "Caesura.AudioBackend", registry.getAudioBackend());
    setRegistryLightUserData(L, "Caesura.PlatformBackend", registry.getPlatformBackend());
    setRegistryLightUserData(L, "Caesura.InputRouter", inputRouter);
    setRegistryLightUserData(L, "Caesura.VideoPlayer", videoPlayer);
    setRegistryLightUserData(L, "Caesura.ParticleSystem", particleSystem);
    setRegistryLightUserData(L, "Caesura.TextureManager", textureManager);
    setRegistryLightUserData(L, "Caesura.AsyncLoader", asyncLoader);
    setRegistryLightUserData(L, "Caesura.DebugManager",
                             static_cast<IDebugManager*>(&DebugManager::instance()));
}

void registerMiniGameLuaRegistryService(lua_State* L, IMiniGameBackend* miniGameBackend) {
    if (!L) return;
    setRegistryLightUserData(L, "Caesura.MiniGameBackend", miniGameBackend);
}

} // namespace Caesura
