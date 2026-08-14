#include "BgfxMiniGameBackend.h"
#include "../di/BackendRegistry.h"
#include "../debug/api/DebugLog.h"
#include "EmbeddedMiniGameShaders.h"
#include <nlohmann_json.hpp>
#include <fstream>
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <bx/readerwriter.h>
#include <cstdio>
#include <cstring>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace Caesura {

// -- bgfx shader binary wrapper (same pattern as BgfxRenderDevice) ---------
static bgfx::ShaderHandle buildMiniGameShader(const uint8_t* bytecode, uint32_t codeSize,
                                              bool fragment, uint8_t numAttrs, const uint16_t* attrIds) {
    if (codeSize > 65536) {
        DEBUG_ERR(SubSys::MiniGame, ErrCode::Ok, "[MiniGame] Shader rejected: %u bytes exceeds 64 KB limit.", codeSize);
        return BGFX_INVALID_HANDLE;
    }
    const uint16_t uniformCount = 0;
    const uint32_t totalSize = 4 + 4 + 4 + 2 + 4 + codeSize + 1 + 1 + 2 * numAttrs + 2;
    const bgfx::Memory* mem = bgfx::alloc(totalSize);
    if (!mem) {
        DEBUG_ERR(SubSys::MiniGame, ErrCode::Ok, "[MiniGame] bgfx::alloc failed for %u bytes", totalSize);
        return BGFX_INVALID_HANDLE;
    }
    bx::StaticMemoryBlockWriter writer(mem->data, mem->size);
    bx::ErrorAssert err;
    const uint32_t magic = fragment
        ? BX_MAKEFOURCC('F', 'S', 'H', 11)
        : BX_MAKEFOURCC('V', 'S', 'H', 11);
    bx::write(&writer, magic, err);
    bx::write(&writer, uint32_t(0), err);
    bx::write(&writer, uint32_t(0), err);
    bx::write(&writer, uniformCount, err);
    bx::write(&writer, codeSize, err);
    for (uint32_t i = 0; i < codeSize; ++i) bx::write(&writer, bytecode[i], err);
    bx::write(&writer, uint8_t(0), err);
    bx::write(&writer, numAttrs, err);
    for (uint8_t i = 0; i < numAttrs; ++i) bx::write(&writer, attrIds[i], err);
    bx::write(&writer, uint16_t(0), err);
    return bgfx::createShader(mem);
}

BgfxMiniGameBackend::~BgfxMiniGameBackend() { shutdown(); }

// ==========================================================================
// Geometry cache
// ==========================================================================

void BgfxMiniGameBackend::initGeometryCache() {
    m_geoVB[int(MiniGeoType::Cube)]   = createVB(createCubeGeometry());
    m_geoIB[int(MiniGeoType::Cube)]   = createIB(createCubeGeometry());
    m_geoVB[int(MiniGeoType::Sphere)] = createVB(createSphereGeometry(16));
    m_geoIB[int(MiniGeoType::Sphere)] = createIB(createSphereGeometry(16));
    m_geoVB[int(MiniGeoType::Plane)]  = createVB(createPlaneGeometry(10,10,4,4));
    m_geoIB[int(MiniGeoType::Plane)]  = createIB(createPlaneGeometry(10,10,4,4));
    m_geoVB[int(MiniGeoType::Quad)]   = createVB(createQuadGeometry());
    m_geoIB[int(MiniGeoType::Quad)]   = createIB(createQuadGeometry());
}

// ==========================================================================
// Lifecycle
// ==========================================================================

bool BgfxMiniGameBackend::init() {
    // GPU resources created lazily by ensureGpuResources()
    return true;
}

bool BgfxMiniGameBackend::ensureGpuResources() {
    if (m_gpuReady) return true;

    // Guard: all shader/uniform/geometry creation below calls bgfx APIs,
    // which is undefined behaviour before bgfx::init. The render device is
    // authoritative for GPU availability (engine inits it before wiring the
    // mini-game backend; tests may pass an uninitialized/null device).
    if (!m_renderDevice || !m_renderDevice->isInitialized()) {
        DEBUG_ERR(SubSys::MiniGame, ErrCode::Ok, "[MiniGame] ensureGpuResources: render device not "
                        "initialized; refusing GPU resource creation");
        return false;
    }

    bgfx::ShaderHandle vs, fs;
    const bgfx::RendererType::Enum rt = bgfx::getRendererType();
    bool shaderOk = false;
    const uint16_t vsAttrs[] = { 0x0001, 0x0002, 0x0010 };

    // D3D11/D3D12: use pre-compiled DXBC bytecode
    if (rt == bgfx::RendererType::Direct3D11 || rt == bgfx::RendererType::Direct3D12) {
        vs = buildMiniGameShader(kEmbeddedDXBC_MiniGame_VS, uint32_t(kEmbeddedDXBC_MiniGame_VS_size), false, 3, vsAttrs);
        fs = buildMiniGameShader(kEmbeddedDXBC_MiniGame_FS, uint32_t(kEmbeddedDXBC_MiniGame_FS_size), true, 0, nullptr);
        shaderOk = bgfx::isValid(vs) && bgfx::isValid(fs);
    }
    // OpenGL/GLES: try built-in GLSL compilation (requires bgfx built with BGFX_CONFIG_RENDERER_OPENGL)
    else if (rt == bgfx::RendererType::OpenGL || rt == bgfx::RendererType::OpenGLES) {
        vs = buildMiniGameShader(reinterpret_cast<const uint8_t*>(kEmbeddedGLSL_MiniGame_VS), uint32_t(strlen(kEmbeddedGLSL_MiniGame_VS)), false, 3, vsAttrs);
        fs = buildMiniGameShader(reinterpret_cast<const uint8_t*>(kEmbeddedGLSL_MiniGame_FS), uint32_t(strlen(kEmbeddedGLSL_MiniGame_FS)), true, 0, nullptr);
        shaderOk = bgfx::isValid(vs) && bgfx::isValid(fs);
    }
    // Metal: try built-in MSL compilation
    else if (rt == bgfx::RendererType::Metal) {
        vs = buildMiniGameShader(reinterpret_cast<const uint8_t*>(kEmbeddedMSL_MiniGame_VS), uint32_t(strlen(kEmbeddedMSL_MiniGame_VS)), false, 3, vsAttrs);
        fs = buildMiniGameShader(reinterpret_cast<const uint8_t*>(kEmbeddedMSL_MiniGame_FS), uint32_t(strlen(kEmbeddedMSL_MiniGame_FS)), true, 0, nullptr);
        shaderOk = bgfx::isValid(vs) && bgfx::isValid(fs);
    }

    // Fallback: try DXBC for any other backend (including Vulkan via bgfx translation)
    if (!shaderOk) {
        vs = buildMiniGameShader(kEmbeddedDXBC_MiniGame_VS, uint32_t(kEmbeddedDXBC_MiniGame_VS_size), false, 3, vsAttrs);
        fs = buildMiniGameShader(kEmbeddedDXBC_MiniGame_FS, uint32_t(kEmbeddedDXBC_MiniGame_FS_size), true, 0, nullptr);
        shaderOk = bgfx::isValid(vs) && bgfx::isValid(fs);
    }

    if (!shaderOk) {
        DEBUG_ERR(SubSys::MiniGame, ErrCode::Ok, "[MiniGame] Shader build failed for renderer %s",
               bgfx::getRendererName(rt));
        if (bgfx::isValid(vs)) bgfx::destroy(vs);
        if (bgfx::isValid(fs)) bgfx::destroy(fs);
        return false;
    }
    m_program = bgfx::createProgram(vs, fs, true);
    if (!bgfx::isValid(m_program)) { printf("[MiniGame] Shader program failed\n"); return false; }

    m_u_mtx       = bgfx::createUniform("u_mtx",       bgfx::UniformType::Mat4);
    m_u_albedo    = bgfx::createUniform("u_albedo",    bgfx::UniformType::Vec4);
    m_u_lightDir  = bgfx::createUniform("u_lightDir",  bgfx::UniformType::Vec4);
    m_u_lightColor= bgfx::createUniform("u_lightColor",bgfx::UniformType::Vec4);
    m_u_ambient   = bgfx::createUniform("u_ambient",   bgfx::UniformType::Vec4);
    m_u_cameraPos = bgfx::createUniform("u_cameraPos", bgfx::UniformType::Vec4);
    // NOTE: "u_viewProj" is a bgfx PredefinedUniform name (auto-bound camera
    // matrix); user code must not create a uniform with that name, or
    // bgfx::createUniform fails identifier validation. Use a custom name.
    m_u_viewProj  = bgfx::createUniform("u_miniViewProj", bgfx::UniformType::Mat4);
    m_u_material  = bgfx::createUniform("u_material",  bgfx::UniformType::Vec4);
    m_u_lightPos[0]=bgfx::createUniform("u_lightPos0", bgfx::UniformType::Vec4);
    m_u_lightCol[0]=bgfx::createUniform("u_lightCol0", bgfx::UniformType::Vec4);
    m_u_lightPos[1]=bgfx::createUniform("u_lightPos1", bgfx::UniformType::Vec4);
    m_u_lightCol[1]=bgfx::createUniform("u_lightCol1", bgfx::UniformType::Vec4);
    m_u_lightPos[2]=bgfx::createUniform("u_lightPos2", bgfx::UniformType::Vec4);
    m_u_lightCol[2]=bgfx::createUniform("u_lightCol2", bgfx::UniformType::Vec4);
    m_u_lightCount =bgfx::createUniform("u_lightCount", bgfx::UniformType::Vec4);

    float d[4]={0.577f,0.577f,0.577f,1.0f}, c[4]={1,1,1,1}, a[4]={0.15f,0.15f,0.15f,1}, m[4]={0.5f,0,0.5f,0}, z[4]={0};
    bgfx::setUniform(m_u_lightDir,d); bgfx::setUniform(m_u_lightColor,c);
    bgfx::setUniform(m_u_ambient,a); bgfx::setUniform(m_u_material,m);
    for(int i=0;i<3;++i){bgfx::setUniform(m_u_lightPos[i],z);bgfx::setUniform(m_u_lightCol[i],z);}
    bgfx::setUniform(m_u_lightCount,z);

    initGeometryCache();
    bgfx::setDebug(BGFX_DEBUG_NONE);
    printf("[MiniGame] Init complete (geo x4, PBR, multi-light, collision, physics)\n");
    m_gpuReady = true;
    printf("[MiniGame] GPU resources ready\n");
    return true;
}

void BgfxMiniGameBackend::shutdown() {
    if (!m_gpuReady) return;
    for(int i=0;i<int(MiniGeoType::Count);++i){if(bgfx::isValid(m_geoVB[i]))bgfx::destroy(m_geoVB[i]);if(bgfx::isValid(m_geoIB[i]))bgfx::destroy(m_geoIB[i]);}
    if(bgfx::isValid(m_program))bgfx::destroy(m_program);
    if(bgfx::isValid(m_u_mtx))bgfx::destroy(m_u_mtx);
    if(bgfx::isValid(m_u_albedo))bgfx::destroy(m_u_albedo);
    if(bgfx::isValid(m_u_lightDir))bgfx::destroy(m_u_lightDir);
    if(bgfx::isValid(m_u_lightColor))bgfx::destroy(m_u_lightColor);
    if(bgfx::isValid(m_u_ambient))bgfx::destroy(m_u_ambient);
    if(bgfx::isValid(m_u_cameraPos))bgfx::destroy(m_u_cameraPos);
    if(bgfx::isValid(m_u_viewProj))bgfx::destroy(m_u_viewProj);
    if(bgfx::isValid(m_u_material))bgfx::destroy(m_u_material);
    for(int i=0;i<3;++i){if(bgfx::isValid(m_u_lightPos[i]))bgfx::destroy(m_u_lightPos[i]);if(bgfx::isValid(m_u_lightCol[i]))bgfx::destroy(m_u_lightCol[i]);}
    if(bgfx::isValid(m_u_lightCount))bgfx::destroy(m_u_lightCount);
    m_objects.clear(); m_materials.clear(); m_pointLights.clear();
    m_active=false; m_gpuReady=false;
}

// ==========================================================================
// Lights
// ==========================================================================

void BgfxMiniGameBackend::setLightUniforms() { if(!m_gpuReady) return;
    float d[4]={m_dirLight.dirX,m_dirLight.dirY,m_dirLight.dirZ,m_dirLight.intensity};
    float c[4]={m_dirLight.r,m_dirLight.g,m_dirLight.b,1};
    float a[4]={m_ambientLight.r,m_ambientLight.g,m_ambientLight.b,1};
    bgfx::setUniform(m_u_lightDir,d); bgfx::setUniform(m_u_lightColor,c); bgfx::setUniform(m_u_ambient,a);
    int n=int(m_pointLights.size());
    for(int i=0;i<3;++i){
        if(i<n){auto&pl=m_pointLights[i];float p[4]={pl.posX,pl.posY,pl.posZ,pl.range};float cl[4]={pl.r,pl.g,pl.b,pl.intensity};bgfx::setUniform(m_u_lightPos[i],p);bgfx::setUniform(m_u_lightCol[i],cl);}
        else{float z[4]={0};bgfx::setUniform(m_u_lightPos[i],z);bgfx::setUniform(m_u_lightCol[i],z);}
    }
    float lc[4]={float(n),0,0,0}; bgfx::setUniform(m_u_lightCount,lc);
}

// ==========================================================================
// Collision
// ==========================================================================

void BgfxMiniGameBackend::runCollisionDetection() {
    if(!m_collisionEnabled||m_objects.size()<2)return;
    std::vector<uint32_t> ids; std::vector<float> px,py,pz,sx,sy,sz;
    for(auto&[id,obj]:m_objects){if(!obj.enableCollision)continue;ids.push_back(id);px.push_back(obj.posX);py.push_back(obj.posY);pz.push_back(obj.posZ);sx.push_back(obj.scaleX);sy.push_back(obj.scaleY);sz.push_back(obj.scaleZ);}
    if(ids.size()<2)return;
    auto pairs=findCollisions(ids.data(),px.data(),py.data(),pz.data(),sx.data(),sy.data(),sz.data(),ids.size());
    if(m_L&&!pairs.empty()){lua_getglobal(m_L,"on_collision");if(lua_isfunction(m_L,-1)){for(auto&p:pairs){lua_pushvalue(m_L,-1);lua_pushinteger(m_L,p.first);lua_pushinteger(m_L,p.second);if(lua_pcall(m_L,2,0,0)!=LUA_OK){printf("[MiniGame] on_collision error: %s\n",lua_tostring(m_L,-1));lua_pop(m_L,1);}}}lua_pop(m_L,1);}
}

bool BgfxMiniGameBackend::checkCollision(uint32_t a,uint32_t b){
    auto ia=m_objects.find(a),ib=m_objects.find(b);
    if(ia==m_objects.end()||ib==m_objects.end())return false;
    AABB aa=computeAABB(ia->second.posX,ia->second.posY,ia->second.posZ,ia->second.scaleX,ia->second.scaleY,ia->second.scaleZ);
    AABB bb=computeAABB(ib->second.posX,ib->second.posY,ib->second.posZ,ib->second.scaleX,ib->second.scaleY,ib->second.scaleZ);
    return aabbOverlap(aa,bb);
}

// ==========================================================================
// Scene
// ==========================================================================

namespace {

using json = nlohmann::json;

void parseCamera(const json& cam, MiniScene& out) {
    if (cam.contains("eye") && cam["eye"].is_array() && cam["eye"].size() >= 3) {
        out.eyeX = cam["eye"][0].get<float>();
        out.eyeY = cam["eye"][1].get<float>();
        out.eyeZ = cam["eye"][2].get<float>();
    }
    if (cam.contains("at") && cam["at"].is_array() && cam["at"].size() >= 3) {
        out.atX = cam["at"][0].get<float>();
        out.atY = cam["at"][1].get<float>();
        out.atZ = cam["at"][2].get<float>();
    }
}

void parseLights(const json& lights, MiniScene& out) {
    if (lights.contains("ambient") && lights["ambient"].is_array() &&
        lights["ambient"].size() >= 3) {
        out.lights.ambient[0] = lights["ambient"][0].get<float>();
        out.lights.ambient[1] = lights["ambient"][1].get<float>();
        out.lights.ambient[2] = lights["ambient"][2].get<float>();
    }
    if (lights.contains("directional") && lights["directional"].is_object()) {
        const auto& dir = lights["directional"];
        out.lights.hasDirectional = true;
        if (dir.contains("dir") && dir["dir"].is_array() && dir["dir"].size() >= 3) {
            out.lights.dir[0] = dir["dir"][0].get<float>();
            out.lights.dir[1] = dir["dir"][1].get<float>();
            out.lights.dir[2] = dir["dir"][2].get<float>();
        }
        if (dir.contains("color") && dir["color"].is_array() && dir["color"].size() >= 3) {
            out.lights.dirColor[0] = dir["color"][0].get<float>();
            out.lights.dirColor[1] = dir["color"][1].get<float>();
            out.lights.dirColor[2] = dir["color"][2].get<float>();
        }
        if (dir.contains("intensity")) out.lights.dirIntensity = dir["intensity"].get<float>();
    }
}

void parseObjectItem(const json& item, uint32_t& nextId, MiniScene& out) {
    if (!item.is_object()) return;
    MiniObject obj;
    obj.id = nextId++;
    const std::string type = item.contains("type") && item["type"].is_string()
        ? item["type"].get<std::string>() : "cube";
    if (type == "sphere")      obj.geoType = MiniGeoType::Sphere;
    else if (type == "plane")  obj.geoType = MiniGeoType::Plane;
    else if (type == "quad")   obj.geoType = MiniGeoType::Quad;
    else                       obj.geoType = MiniGeoType::Cube;
    const auto readVec = [&item](const char* key, float& x, float& y, float& z) {
        if (!item.contains(key) || !item[key].is_array() || item[key].size() < 3) return;
        x = item[key][0].get<float>();
        y = item[key][1].get<float>();
        z = item[key][2].get<float>();
    };
    readVec("pos", obj.posX, obj.posY, obj.posZ);
    readVec("rot", obj.rotX, obj.rotY, obj.rotZ);
    if (item.contains("scale") && item["scale"].is_array() && item["scale"].size() >= 3) {
        obj.scaleX = item["scale"][0].get<float>();
        obj.scaleY = item["scale"][1].get<float>();
        obj.scaleZ = item["scale"][2].get<float>();
    } else if (item.contains("scale") && item["scale"].is_number()) {
        const float s = item["scale"].get<float>();
        obj.scaleX = obj.scaleY = obj.scaleZ = s;
    }
    if (item.contains("color") && item["color"].is_array() && item["color"].size() >= 3) {
        obj.r = item["color"][0].get<float>();
        obj.g = item["color"][1].get<float>();
        obj.b = item["color"][2].get<float>();
    }
    if (item.contains("gravity") && item["gravity"].is_boolean()) {
        obj.useGravity = item["gravity"].get<bool>();
    }
    if (item.contains("material") && item["material"].is_number()) {
        obj.materialId = item["material"].get<uint32_t>();
    }
    out.objects.push_back(obj);
}

} // namespace

bool BgfxMiniGameBackend::sceneFromJson(const std::string& jsonText, MiniScene& out) {
    try {
        const json doc = json::parse(jsonText);
        if (doc.contains("name") && doc["name"].is_string()) {
            out.name = doc["name"].get<std::string>();
        }
        if (doc.contains("camera") && doc["camera"].is_object()) {
            parseCamera(doc["camera"], out);
        }
        if (doc.contains("lights") && doc["lights"].is_object()) {
            parseLights(doc["lights"], out);
        }
        if (doc.contains("objects") && doc["objects"].is_array()) {
            for (const auto& item : doc["objects"]) {
                parseObjectItem(item, m_nextObjId, out);
            }
        }
        return true;
    } catch (const std::exception& error) {
        DEBUG_ERR(SubSys::MiniGame, ErrCode::Ok, "[MiniGame] sceneFromJson failed: %s", error.what());
        return false;
    }
}

uint32_t BgfxMiniGameBackend::loadScene(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        DEBUG_ERR(SubSys::MiniGame, ErrCode::Ok, "[MiniGame] loadScene: cannot open %s", path.c_str());
        return 0;
    }
    std::string jsonText((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    MiniScene scene;
    if (!sceneFromJson(jsonText, scene)) {
        DEBUG_ERR(SubSys::MiniGame, ErrCode::Ok, "[MiniGame] loadScene: invalid scene file %s", path.c_str());
        return 0;
    }
    scene.id = m_nextSceneId++;
    m_scenes[scene.id] = std::move(scene);
    printf("[MiniGame] loadScene: %s -> scene %u (%zu objects)\n",
           path.c_str(), scene.id, m_scenes[scene.id].objects.size());
    return scene.id;
}

void BgfxMiniGameBackend::unloadScene(uint32_t h) {
    auto it = m_scenes.find(h);
    if (it == m_scenes.end()) return;
    auto spawned = m_sceneObjects.find(h);
    if (spawned != m_sceneObjects.end()) {
        for (uint32_t objId : spawned->second) m_objects.erase(objId);
        m_sceneObjects.erase(spawned);
    }
    if (h == m_activeScene) {
        m_active = false;
        m_activeScene = 0;
        auto* router = BackendRegistry::instance().getInputRouter();
        if (router) router->setFocus(InputFocus::KAG);
    }
    m_scenes.erase(it);
}

void BgfxMiniGameBackend::enter(uint32_t h){
    if (h == 0) {
        // Programmatic mode: activate whatever objects were spawned via the
        // Lua API (spawn_cube/sphere/plane) without a JSON scene descriptor.
        // leave() deactivates; render() draws the current m_objects set.
        if (!ensureGpuResources()) return;
        if (m_activeScene != 0) {
            // Switch away from a JSON scene: drop its spawned objects.
            m_active = false;
            auto spawned = m_sceneObjects.find(m_activeScene);
            if (spawned != m_sceneObjects.end()) {
                for (uint32_t objId : spawned->second) m_objects.erase(objId);
                m_sceneObjects.erase(spawned);
            }
        }
        m_activeScene = 0;
        m_active = true;
        printf("[MiniGame] Programmatic scene activated (%zu objects)\n",
               m_objects.size());
        return;
    }
    auto it = m_scenes.find(h);
    if (it == m_scenes.end()) {
        DEBUG_ERR(SubSys::MiniGame, ErrCode::Ok, "[MiniGame] enter: unknown scene %u", h);
        return;
    }
    if(!ensureGpuResources())return;

    // Leave the previous scene first (remove its objects)
    if (m_activeScene != 0 && m_activeScene != h) {
        const uint32_t prev = m_activeScene;
        m_active = false;
        auto spawned = m_sceneObjects.find(prev);
        if (spawned != m_sceneObjects.end()) {
            for (uint32_t objId : spawned->second) m_objects.erase(objId);
            m_sceneObjects.erase(spawned);
        }
    }

    const MiniScene& scene = it->second;
    m_camera.eyeX = scene.eyeX; m_camera.eyeY = scene.eyeY; m_camera.eyeZ = scene.eyeZ;
    m_camera.atX  = scene.atX;  m_camera.atY  = scene.atY;  m_camera.atZ  = scene.atZ;
    m_ambientLight.r = scene.lights.ambient[0];
    m_ambientLight.g = scene.lights.ambient[1];
    m_ambientLight.b = scene.lights.ambient[2];
    if (scene.lights.hasDirectional) {
        m_dirLight.dirX = scene.lights.dir[0]; m_dirLight.dirY = scene.lights.dir[1]; m_dirLight.dirZ = scene.lights.dir[2];
        m_dirLight.r = scene.lights.dirColor[0]; m_dirLight.g = scene.lights.dirColor[1]; m_dirLight.b = scene.lights.dirColor[2];
        m_dirLight.intensity = scene.lights.dirIntensity;
    }

    std::vector<uint32_t> spawned;
    spawned.reserve(scene.objects.size());
    for (const MiniObject& obj : scene.objects) {
        m_objects[obj.id] = obj;
        spawned.push_back(obj.id);
    }
    m_sceneObjects[h] = std::move(spawned);

    m_activeScene = h;
    m_active = true;
    // D9.4: Switch input focus to GAME when entering mini-game
    auto* router = BackendRegistry::instance().getInputRouter();
    if (router) router->setFocus(InputFocus::GAME);
}
void BgfxMiniGameBackend::leave(){
    m_active=false;
    // D9.4: Switch input focus back to KAG when leaving mini-game
    auto* router = BackendRegistry::instance().getInputRouter();
    if (router) router->setFocus(InputFocus::KAG);
}

// ==========================================================================
// Game loop
// ==========================================================================

bool BgfxMiniGameBackend::update(float dt) {
    for(auto&[id,obj]:m_objects){if(obj.useGravity)obj.accelY+=m_gravity;obj.velX+=obj.accelX*dt;obj.velY+=obj.accelY*dt;obj.velZ+=obj.accelZ*dt;obj.posX+=obj.velX*dt;obj.posY+=obj.velY*dt;obj.posZ+=obj.velZ*dt;obj.accelX=0;obj.accelY=0;obj.accelZ=0;}
    runCollisionDetection();
    return true;
}

void BgfxMiniGameBackend::render() { if(!m_gpuReady) return;
    if(!m_active)return;
    // P1-4: the view needs a rect and a clear or bgfx may skip the submit
    // (a view with no rect never rasterizes). Match the backbuffer so the
    // 3D scene fills the window on any resolution.
    const bgfx::Stats* stats = bgfx::getStats();
    const uint16_t vw = stats ? static_cast<uint16_t>(stats->width) : 1280;
    const uint16_t vh = stats ? static_cast<uint16_t>(stats->height) : 720;
    bgfx::setViewRect(MINIGAME_VIEW, 0, 0, vw, vh);
    bgfx::setViewClear(MINIGAME_VIEW, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x001a1a2e, 1.0f, 0);
    float view[16],proj[16],viewProj[16];
    bx::mtxLookAt(view,bx::Vec3{m_camera.eyeX,m_camera.eyeY,m_camera.eyeZ},bx::Vec3{m_camera.atX,m_camera.atY,m_camera.atZ});
    const float aspect = (stats && stats->height > 0)
        ? static_cast<float>(stats->width) / static_cast<float>(stats->height)
        : 16.0f / 9.0f;
    bx::mtxProj(proj,m_camera.fov,aspect,m_camera.nearPlane,m_camera.farPlane,bgfx::getCaps()->homogeneousDepth);
    bx::mtxMul(viewProj,view,proj);
    bgfx::setViewTransform(MINIGAME_VIEW,view,proj);
    float cp[4]={m_camera.eyeX,m_camera.eyeY,m_camera.eyeZ,1};bgfx::setUniform(m_u_cameraPos,cp);bgfx::setUniform(m_u_viewProj,viewProj);
    setLightUniforms();
    for(auto&[id,obj]:m_objects)submitObject(obj);
}

void BgfxMiniGameBackend::submitObject(const MiniObject& obj) {
    float model[16];
    bx::mtxSRT(model,obj.scaleX,obj.scaleY,obj.scaleZ,bx::toRad(obj.rotX),bx::toRad(obj.rotY),bx::toRad(obj.rotZ),obj.posX,obj.posY,obj.posZ);
    bgfx::setTransform(model); bgfx::setUniform(m_u_mtx,model);
    float alb[4]={obj.r,obj.g,obj.b,1}, mat[4]={0.5f,0,0.5f,0};
    if(obj.materialId){auto it=m_materials.find(obj.materialId);if(it!=m_materials.end()){alb[0]=it->second.r;alb[1]=it->second.g;alb[2]=it->second.b;mat[0]=it->second.roughness;mat[1]=it->second.metallic;mat[2]=it->second.specular;}}
    bgfx::setUniform(m_u_albedo,alb); bgfx::setUniform(m_u_material,mat);
    uint64_t st=BGFX_STATE_DEFAULT|BGFX_STATE_WRITE_RGB|BGFX_STATE_WRITE_A|BGFX_STATE_DEPTH_TEST_LESS|BGFX_STATE_WRITE_Z|BGFX_STATE_CULL_CW|BGFX_STATE_MSAA;
    int gi=int(obj.geoType); if(gi<0||gi>=int(MiniGeoType::Count))return;
    bgfx::setState(st); bgfx::setVertexBuffer(0,m_geoVB[gi]); bgfx::setIndexBuffer(m_geoIB[gi]);
    bgfx::submit(MINIGAME_VIEW,m_program);
}

bool BgfxMiniGameBackend::processEvent(const void* e){(void)e;return false;}

// ==========================================================================
// Lua dispatch
// ==========================================================================



int BgfxMiniGameBackend::luaCall(lua_State* L,const char* method){
    for(const LuaMethod& entry : kLuaMethods){
        if(strcmp(method,entry.name)==0)return entry.fn(*this,L);
    }
    printf("[MiniGame] Unknown: %s\n",method);lua_pushboolean(L,0);return 1;
}

int BgfxMiniGameBackend::luaSpawnCube(BgfxMiniGameBackend& self,lua_State* L){
    lua_pushinteger(L,self.spawnCube((float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_optnumber(L,5,1),(float)luaL_optnumber(L,6,1),(float)luaL_optnumber(L,7,1),(float)luaL_optnumber(L,8,1),(uint32_t)luaL_optinteger(L,9,0)));return 1;
}
int BgfxMiniGameBackend::luaSpawnSphere(BgfxMiniGameBackend& self,lua_State* L){
    lua_pushinteger(L,self.spawnSphere((float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_optnumber(L,5,1),(float)luaL_optnumber(L,6,1),(float)luaL_optnumber(L,7,1),(float)luaL_optnumber(L,8,1),(uint32_t)luaL_optinteger(L,9,0)));return 1;
}
int BgfxMiniGameBackend::luaSpawnPlane(BgfxMiniGameBackend& self,lua_State* L){
    lua_pushinteger(L,self.spawnPlane((float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_optnumber(L,5,10),(float)luaL_optnumber(L,6,10),(float)luaL_optnumber(L,7,.5),(float)luaL_optnumber(L,8,.5),(float)luaL_optnumber(L,9,.5),(uint32_t)luaL_optinteger(L,10,0)));return 1;
}
int BgfxMiniGameBackend::luaRemoveObject(BgfxMiniGameBackend& self,lua_State* L){
    self.removeObject((uint32_t)luaL_checkinteger(L,2));lua_pushboolean(L,1);return 1;
}
int BgfxMiniGameBackend::luaSetCamera(BgfxMiniGameBackend& self,lua_State* L){
    self.setCamera((float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_checknumber(L,5),(float)luaL_checknumber(L,6),(float)luaL_checknumber(L,7));lua_pushboolean(L,1);return 1;
}
int BgfxMiniGameBackend::luaCreateMaterial(BgfxMiniGameBackend& self,lua_State* L){
    lua_pushinteger(L,self.createMaterial((float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_optnumber(L,5,.5),(float)luaL_optnumber(L,6,0),(float)luaL_optnumber(L,7,.5),luaL_optstring(L,8,"")));return 1;
}
int BgfxMiniGameBackend::luaSetMaterial(BgfxMiniGameBackend& self,lua_State* L){
    lua_pushboolean(L,self.setObjectMaterial((uint32_t)luaL_checkinteger(L,2),(uint32_t)luaL_checkinteger(L,3))?1:0);return 1;
}
int BgfxMiniGameBackend::luaSetAmbient(BgfxMiniGameBackend& self,lua_State* L){
    self.setAmbient((float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4));lua_pushboolean(L,1);return 1;
}
int BgfxMiniGameBackend::luaSetDirectional(BgfxMiniGameBackend& self,lua_State* L){
    self.setDirectional((float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_optnumber(L,5,1),(float)luaL_optnumber(L,6,1),(float)luaL_optnumber(L,7,1),(float)luaL_optnumber(L,8,1));lua_pushboolean(L,1);return 1;
}
int BgfxMiniGameBackend::luaAddPointLight(BgfxMiniGameBackend& self,lua_State* L){
    lua_pushinteger(L,self.addPointLight((float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_optnumber(L,5,1),(float)luaL_optnumber(L,6,1),(float)luaL_optnumber(L,7,1),(float)luaL_optnumber(L,8,1),(float)luaL_optnumber(L,9,10),luaL_optstring(L,10,"")));return 1;
}
int BgfxMiniGameBackend::luaRemoveLight(BgfxMiniGameBackend& self,lua_State* L){
    lua_pushboolean(L,self.removeLight((uint32_t)luaL_checkinteger(L,2))?1:0);return 1;
}
int BgfxMiniGameBackend::luaCheckCollision(BgfxMiniGameBackend& self,lua_State* L){
    lua_pushboolean(L,self.checkCollision((uint32_t)luaL_checkinteger(L,2),(uint32_t)luaL_checkinteger(L,3))?1:0);return 1;
}
int BgfxMiniGameBackend::luaSetCollision(BgfxMiniGameBackend& self,lua_State* L){
    self.m_collisionEnabled=lua_toboolean(L,2)!=0;lua_pushboolean(L,1);return 1;
}
int BgfxMiniGameBackend::luaSetVelocity(BgfxMiniGameBackend& self,lua_State* L){
    self.setVelocity((uint32_t)luaL_checkinteger(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_checknumber(L,5));lua_pushboolean(L,1);return 1;
}
int BgfxMiniGameBackend::luaSetGravity(BgfxMiniGameBackend& self,lua_State* L){
    self.setGravity((uint32_t)luaL_checkinteger(L,2),lua_toboolean(L,3)!=0);lua_pushboolean(L,1);return 1;
}

// ==========================================================================
// Object API
// ==========================================================================

uint32_t BgfxMiniGameBackend::spawnCube(float x,float y,float z,float s,float r,float g,float b,uint32_t m){
    uint32_t id=m_nextObjId++; MiniObject o; o.id=id; o.posX=x;o.posY=y;o.posZ=z;
    o.scaleX=s;o.scaleY=s;o.scaleZ=s; o.r=r;o.g=g;o.b=b; o.materialId=m; o.geoType=MiniGeoType::Cube;
    m_objects[id]=o; return id;
}
uint32_t BgfxMiniGameBackend::spawnSphere(float x,float y,float z,float rad,float r,float g,float b,uint32_t m){
    uint32_t id=m_nextObjId++; MiniObject o; o.id=id; o.posX=x;o.posY=y;o.posZ=z;
    o.scaleX=rad;o.scaleY=rad;o.scaleZ=rad; o.r=r;o.g=g;o.b=b; o.materialId=m; o.geoType=MiniGeoType::Sphere;
    m_objects[id]=o; return id;
}
uint32_t BgfxMiniGameBackend::spawnPlane(float x,float y,float z,float w,float h,float r,float g,float b,uint32_t m){
    uint32_t id=m_nextObjId++; MiniObject o; o.id=id; o.posX=x;o.posY=y;o.posZ=z;
    o.scaleX=w;o.scaleY=1;o.scaleZ=h; o.r=r;o.g=g;o.b=b; o.materialId=m; o.geoType=MiniGeoType::Plane;
    m_objects[id]=o; return id;
}
void BgfxMiniGameBackend::removeObject(uint32_t id){m_objects.erase(id);}
void BgfxMiniGameBackend::setCamera(float ex,float ey,float ez,float ax,float ay,float az){
    m_camera.eyeX=ex;m_camera.eyeY=ey;m_camera.eyeZ=ez;m_camera.atX=ax;m_camera.atY=ay;m_camera.atZ=az;
}

// ==========================================================================
// Material
// ==========================================================================

uint32_t BgfxMiniGameBackend::createMaterial(float r,float g,float b,float rough,float metal,float spec,const char* name){
    uint32_t id=m_nextMaterialId++; MiniMaterial mat;
    mat.id=id;mat.name=name?name:"";mat.r=r;mat.g=g;mat.b=b;mat.roughness=rough;mat.metallic=metal;mat.specular=spec;
    m_materials[id]=mat;return id;
}
bool BgfxMiniGameBackend::setObjectMaterial(uint32_t oid,uint32_t mid){
    auto it=m_objects.find(oid);if(it==m_objects.end())return false;it->second.materialId=mid;return true;
}

// ==========================================================================
// Lighting
// ==========================================================================

void BgfxMiniGameBackend::setAmbient(float r,float g,float b){m_ambientLight.r=r;m_ambientLight.g=g;m_ambientLight.b=b;}
void BgfxMiniGameBackend::setDirectional(float dx,float dy,float dz,float r,float g,float b,float i){
    m_dirLight.dirX=dx;m_dirLight.dirY=dy;m_dirLight.dirZ=dz;m_dirLight.r=r;m_dirLight.g=g;m_dirLight.b=b;m_dirLight.intensity=i;
}
uint32_t BgfxMiniGameBackend::addPointLight(float x,float y,float z,float r,float g,float b,float intensity,float range,const char* name){
    if((int)m_pointLights.size()>=MAX_POINT_LIGHTS)return 0;
    uint32_t id=m_nextLightId++; MiniPointLight pl;
    pl.id=id;pl.name=name?name:"";pl.posX=x;pl.posY=y;pl.posZ=z;pl.r=r;pl.g=g;pl.b=b;pl.intensity=intensity;pl.range=range;
    m_pointLights.push_back(pl);return id;
}
bool BgfxMiniGameBackend::removeLight(uint32_t id){
    auto it=std::find_if(m_pointLights.begin(),m_pointLights.end(),[id](auto&l){return l.id==id;});
    if(it==m_pointLights.end())return false;m_pointLights.erase(it);return true;
}

// ==========================================================================
// Physics
// ==========================================================================

void BgfxMiniGameBackend::setVelocity(uint32_t objId,float vx,float vy,float vz){
    auto it=m_objects.find(objId);if(it==m_objects.end())return;it->second.velX=vx;it->second.velY=vy;it->second.velZ=vz;
}
void BgfxMiniGameBackend::setGravity(uint32_t objId,bool enabled){
    auto it=m_objects.find(objId);if(it==m_objects.end())return;it->second.useGravity=enabled;
}

} // namespace Caesura
