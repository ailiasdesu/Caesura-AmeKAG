#pragma once
#include "IMiniGameBackend.h"
#include "../Render/IRenderDevice.h"
#include <bgfx/bgfx.h>
#include <vector>
#include <unordered_map>

namespace Caesura {

// ===========================================================================
// BgfxMiniGameBackend — concrete 3D mini-game backend via bgfx
// ===========================================================================
// Renders simple 3D primitives (cubes, spheres) into a dedicated bgfx view.
// Supports basic orbit camera + Lua spawning API.
//
// Shader: embedded minimal 3D vertex/fragment (DXBC/GLSL variants).

struct MiniCamera {
    float eyeX = 0, eyeY = 3, eyeZ = 8;   // orbit camera position
    float atX = 0, atY = 0, atZ = 0;      // look-at target
    float fov  = 60.0f;                    // vertical FOV degrees
    float nearPlane = 0.1f, farPlane = 100.0f;
};

struct MiniObject {
    uint32_t id;
    float posX = 0, posY = 0, posZ = 0;
    float scaleX = 1, scaleY = 1, scaleZ = 1;
    float rotX = 0, rotY = 0, rotZ = 0;
    float r = 1, g = 1, b = 1;  // color
};

class BgfxMiniGameBackend : public IMiniGameBackend {
public:
    BgfxMiniGameBackend() = default;
    ~BgfxMiniGameBackend() override;

    bool init() override;
    void shutdown() override;

    uint32_t loadScene(const std::string& path) override;
    void unloadScene(uint32_t sceneHandle) override;
    void enter(uint32_t sceneHandle) override;
    void leave() override;
    bool isActive() const override { return m_active; }

    bool update(float deltaTime) override;
    void render() override;
    bool processEvent(const void* sdlEvent) override;
    int  luaCall(lua_State* L, const char* method) override;
    const char* getBackendName() const override { return "bgfx-3d"; }

    // -- Lua-accessible API (called via luaCall dispatch) ------------------
    uint32_t spawnCube(float x, float y, float z, float size,
                       float r, float g, float b);
    void     removeObject(uint32_t id);
    void     setCamera(float eyeX, float eyeY, float eyeZ,
                       float atX, float atY, float atZ);
    void     setAmbientLight(float r, float g, float b);

    void setRenderDevice(IRenderDevice* dev) { m_renderDevice = dev; }

private:
    bool loadShaders();
    void submitObject(const MiniObject& obj);

    IRenderDevice* m_renderDevice = nullptr;
    bool m_active = false;
    uint32_t m_activeScene = 0;
    uint32_t m_nextSceneId = 1;
    uint32_t m_nextObjId = 1;

    MiniCamera m_camera;

    // GPU resources
    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_mtx     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_color   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_lightDir = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout  m_layout;
    bgfx::VertexBufferHandle m_cubeVB = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle  m_cubeIB = BGFX_INVALID_HANDLE;

    static constexpr uint16_t MINIGAME_VIEW = 10;  // dedicated bgfx view

    // Scene objects
    std::unordered_map<uint32_t, MiniObject> m_objects;
};

// Lua binding registration
void registerMiniGameBinding(lua_State* L, BgfxMiniGameBackend* backend);

} // namespace Caesura