#pragma once
#include "api/IMiniGameBackend.h"

namespace Caesura {

// ===========================================================================
// NullMiniGameBackend -- no-op implementation
// ===========================================================================
// Always returns success without side effects. Used when no 3D mini-game
// backend is selected. Allows all reserved Lua API calls to succeed silently.
//
// This is the default backend; it gets replaced when a real implementation
// (e.g., BgfxMiniGameBackend) is developed and registered.

class NullMiniGameBackend : public IMiniGameBackend {
public:
    bool init() override;
    void shutdown() override;

    uint32_t loadScene(const std::string& path) override;
    void unloadScene(uint32_t sceneHandle) override;

    void enter(uint32_t sceneHandle) override;
    void leave() override;
    bool isActive() const override;

    bool update(float deltaTime) override;
    void render() override;

    bool processEvent(const void* sdlEvent) override;
    int  luaCall(lua_State* L, const char* method) override;

        void setRenderDevice(class IRenderDevice* dev) override { (void)dev; }

    const char* getBackendName() const override { return "NullMiniGame"; }

private:
    bool m_active = false;
    uint32_t m_nextSceneHandle = 1;
};

} // namespace Caesura
