#pragma once
#include "api/IMiniGameBackend.h"
#include "MiniMaterial.h"
#include "MiniGeometry.h"
#include "MiniLight.h"
#include "MiniCollision.h"
#include "../Render/IRenderDevice.h"
#include <bgfx/bgfx.h>
#include <vector>
#include <unordered_map>

namespace Caesura {

struct MiniCamera {
    float eyeX = 0, eyeY = 3, eyeZ = 8;
    float atX = 0, atY = 0, atZ = 0;
    float fov  = 60.0f;
    float nearPlane = 0.1f, farPlane = 100.0f;
};

struct MiniObject {
    uint32_t id;
    float posX = 0, posY = 0, posZ = 0;
    float scaleX = 1, scaleY = 1, scaleZ = 1;
    float rotX = 0, rotY = 0, rotZ = 0;
    MiniGeoType geoType = MiniGeoType::Cube;
    uint32_t materialId = 0;
    float r = 1, g = 1, b = 1;
    bool enableCollision = true;
    float velX = 0, velY = 0, velZ = 0;
    float accelX = 0, accelY = 0, accelZ = 0;
    bool useGravity = false;
};

class BgfxMiniGameBackend : public IMiniGameBackend {
public:
    BgfxMiniGameBackend() = default;
    ~BgfxMiniGameBackend() override;

    bool init() override;
    bool ensureGpuResources();
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

    // Spawn
    uint32_t spawnCube(float x, float y, float z, float size,
                       float r, float g, float b, uint32_t materialId = 0);
    uint32_t spawnSphere(float x, float y, float z, float radius,
                         float r, float g, float b, uint32_t materialId = 0);
    uint32_t spawnPlane(float x, float y, float z, float w, float h,
                        float r, float g, float b, uint32_t materialId = 0);
    void removeObject(uint32_t id);
    void setCamera(float eyeX, float eyeY, float eyeZ, float atX, float atY, float atZ);

    // Material
    uint32_t createMaterial(float r, float g, float b,
                            float roughness, float metallic, float specular,
                            const char* name);
    bool     setObjectMaterial(uint32_t objId, uint32_t materialId);

    // Lighting
    void     setAmbient(float r, float g, float b);
    void     setDirectional(float dirX, float dirY, float dirZ,
                            float r, float g, float b, float intensity);
    uint32_t addPointLight(float x, float y, float z,
                           float r, float g, float b,
                           float intensity, float range, const char* name);
    bool     removeLight(uint32_t lightId);

    // Collision
    bool     checkCollision(uint32_t objA, uint32_t objB);
    void     setCollisionEnabled(bool e) { m_collisionEnabled = e; }
    bool     isCollisionEnabled() const { return m_collisionEnabled; }
    void     setLuaState(lua_State* L) { m_L = L; }

    // Physics
    void     setVelocity(uint32_t objId, float vx, float vy, float vz);
    void     setGravity(uint32_t objId, bool enabled);
    void     setGravityValue(float g) { m_gravity = g; }

    void setRenderDevice(IRenderDevice* dev) { m_renderDevice = dev; }

private:
    void initGeometryCache();
    void setLightUniforms();
    void submitObject(const MiniObject& obj);
    void runCollisionDetection();

    IRenderDevice* m_renderDevice = nullptr;
    bool m_active = false;
    bool m_gpuReady = false;
    uint32_t m_activeScene = 0;
    uint32_t m_nextSceneId = 1, m_nextObjId = 1;
    MiniCamera m_camera;

    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_mtx = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_albedo = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_lightDir = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_lightColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_ambient = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_cameraPos = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_viewProj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_material = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_lightPos[3] = {BGFX_INVALID_HANDLE};
    bgfx::UniformHandle m_u_lightCol[3] = {BGFX_INVALID_HANDLE};
    bgfx::UniformHandle m_u_lightCount = BGFX_INVALID_HANDLE;

    bgfx::VertexBufferHandle m_geoVB[int(MiniGeoType::Count)] = {};
    bgfx::IndexBufferHandle  m_geoIB[int(MiniGeoType::Count)] = {};

    static constexpr uint16_t MINIGAME_VIEW = 10;
    static constexpr int MAX_POINT_LIGHTS = 3;

    std::unordered_map<uint32_t, MiniObject> m_objects;
    std::unordered_map<uint32_t, MiniMaterial> m_materials;
    uint32_t m_nextMaterialId = 1;

    MiniDirectionalLight m_dirLight;
    MiniAmbientLight m_ambientLight;
    std::vector<MiniPointLight> m_pointLights;
    uint32_t m_nextLightId = 1;

    bool m_collisionEnabled = true;
    float m_gravity = -9.8f;
    lua_State* m_L = nullptr;
};

void registerMiniGameBinding(lua_State* L, BgfxMiniGameBackend* backend);
} // namespace Caesura

