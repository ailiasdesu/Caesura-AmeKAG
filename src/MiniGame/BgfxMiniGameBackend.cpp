#include "BgfxMiniGameBackend.h"
#include "../di/BackendRegistry.h"
#include "../Render/EmbeddedShaders.h"
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
        fprintf(stderr, "[MiniGame] Shader rejected: %u bytes exceeds 64 KB limit.\n", codeSize);
        return BGFX_INVALID_HANDLE;
    }
    const uint16_t uniformCount = 0;
    const uint32_t totalSize = 4 + 4 + 4 + 2 + 4 + codeSize + 1 + 1 + 2 * numAttrs + 2;
    const bgfx::Memory* mem = bgfx::alloc(totalSize);
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
        fprintf(stderr, "[MiniGame] Shader build failed for renderer %s\n",
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
    m_u_viewProj  = bgfx::createUniform("u_viewProj",  bgfx::UniformType::Mat4);
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
    m_active=false;
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

uint32_t BgfxMiniGameBackend::loadScene(const std::string& p){printf("[MiniGame] loadScene: %s\n",p.c_str());return m_nextSceneId++;}
void BgfxMiniGameBackend::unloadScene(uint32_t h){if(h==m_activeScene)leave();}
void BgfxMiniGameBackend::enter(uint32_t h){if(!ensureGpuResources())return;m_activeScene=h;m_active=true;}
void BgfxMiniGameBackend::leave(){m_active=false;}

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
    float view[16],proj[16],viewProj[16];
    bx::mtxLookAt(view,bx::Vec3{m_camera.eyeX,m_camera.eyeY,m_camera.eyeZ},bx::Vec3{m_camera.atX,m_camera.atY,m_camera.atZ});
    float aspect=1280.0f/720.0f;
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
    if(strcmp(method,"spawn_cube")==0){lua_pushinteger(L,spawnCube((float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_optnumber(L,5,1),(float)luaL_optnumber(L,6,1),(float)luaL_optnumber(L,7,1),(float)luaL_optnumber(L,8,1),(uint32_t)luaL_optinteger(L,9,0)));return 1;}
    if(strcmp(method,"spawn_sphere")==0){lua_pushinteger(L,spawnSphere((float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_optnumber(L,5,1),(float)luaL_optnumber(L,6,1),(float)luaL_optnumber(L,7,1),(float)luaL_optnumber(L,8,1),(uint32_t)luaL_optinteger(L,9,0)));return 1;}
    if(strcmp(method,"spawn_plane")==0){lua_pushinteger(L,spawnPlane((float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_optnumber(L,5,10),(float)luaL_optnumber(L,6,10),(float)luaL_optnumber(L,7,.5),(float)luaL_optnumber(L,8,.5),(float)luaL_optnumber(L,9,.5),(uint32_t)luaL_optinteger(L,10,0)));return 1;}
    if(strcmp(method,"remove_object")==0){removeObject((uint32_t)luaL_checkinteger(L,2));lua_pushboolean(L,1);return 1;}
    if(strcmp(method,"set_camera")==0){setCamera((float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_checknumber(L,5),(float)luaL_checknumber(L,6),(float)luaL_checknumber(L,7));lua_pushboolean(L,1);return 1;}
    if(strcmp(method,"create_material")==0){lua_pushinteger(L,createMaterial((float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_optnumber(L,5,.5),(float)luaL_optnumber(L,6,0),(float)luaL_optnumber(L,7,.5),luaL_optstring(L,8,"")));return 1;}
    if(strcmp(method,"set_material")==0){lua_pushboolean(L,setObjectMaterial((uint32_t)luaL_checkinteger(L,2),(uint32_t)luaL_checkinteger(L,3))?1:0);return 1;}
    if(strcmp(method,"set_ambient")==0){setAmbient((float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4));lua_pushboolean(L,1);return 1;}
    if(strcmp(method,"set_directional")==0){setDirectional((float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_optnumber(L,5,1),(float)luaL_optnumber(L,6,1),(float)luaL_optnumber(L,7,1),(float)luaL_optnumber(L,8,1));lua_pushboolean(L,1);return 1;}
    if(strcmp(method,"add_point_light")==0){lua_pushinteger(L,addPointLight((float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_optnumber(L,5,1),(float)luaL_optnumber(L,6,1),(float)luaL_optnumber(L,7,1),(float)luaL_optnumber(L,8,1),(float)luaL_optnumber(L,9,10),luaL_optstring(L,10,"")));return 1;}
    if(strcmp(method,"remove_light")==0){lua_pushboolean(L,removeLight((uint32_t)luaL_checkinteger(L,2))?1:0);return 1;}
    if(strcmp(method,"check_collision")==0){lua_pushboolean(L,checkCollision((uint32_t)luaL_checkinteger(L,2),(uint32_t)luaL_checkinteger(L,3))?1:0);return 1;}
    if(strcmp(method,"set_collision")==0){m_collisionEnabled=lua_toboolean(L,2)!=0;lua_pushboolean(L,1);return 1;}
    if(strcmp(method,"set_velocity")==0){setVelocity((uint32_t)luaL_checkinteger(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_checknumber(L,5));lua_pushboolean(L,1);return 1;}
    if(strcmp(method,"set_gravity")==0){setGravity((uint32_t)luaL_checkinteger(L,2),lua_toboolean(L,3)!=0);lua_pushboolean(L,1);return 1;}
    printf("[MiniGame] Unknown: %s\n",method);lua_pushboolean(L,0);return 1;
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

// ==========================================================================
// Lua bindings
// ==========================================================================

static BgfxMiniGameBackend* g_mg=nullptr;

#define MG(name,body) static int lua_mg_##name(lua_State*L){if(!g_mg){lua_pushinteger(L,0);return 1;}body}
MG(spawn_cube,lua_pushinteger(L,g_mg->spawnCube((float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_optnumber(L,4,1),(float)luaL_optnumber(L,5,1),(float)luaL_optnumber(L,6,1),(float)luaL_optnumber(L,7,1),(uint32_t)luaL_optinteger(L,8,0)));return 1;)
MG(spawn_sphere,lua_pushinteger(L,g_mg->spawnSphere((float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_optnumber(L,4,1),(float)luaL_optnumber(L,5,1),(float)luaL_optnumber(L,6,1),(float)luaL_optnumber(L,7,1),(uint32_t)luaL_optinteger(L,8,0)));return 1;)
MG(spawn_plane,lua_pushinteger(L,g_mg->spawnPlane((float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_optnumber(L,4,10),(float)luaL_optnumber(L,5,10),(float)luaL_optnumber(L,6,.5),(float)luaL_optnumber(L,7,.5),(float)luaL_optnumber(L,8,.5),(uint32_t)luaL_optinteger(L,9,0)));return 1;)
MG(remove_object,g_mg->removeObject((uint32_t)luaL_checkinteger(L,1));return 0;)
MG(set_camera,g_mg->setCamera((float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_checknumber(L,5),(float)luaL_checknumber(L,6));return 0;)
MG(create_material,lua_pushinteger(L,g_mg->createMaterial((float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_optnumber(L,4,.5),(float)luaL_optnumber(L,5,0),(float)luaL_optnumber(L,6,.5),luaL_optstring(L,7,"")));return 1;)
MG(set_material,lua_pushboolean(L,g_mg->setObjectMaterial((uint32_t)luaL_checkinteger(L,1),(uint32_t)luaL_checkinteger(L,2))?1:0);return 1;)
MG(set_ambient,g_mg->setAmbient((float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3));return 0;)
MG(set_directional,g_mg->setDirectional((float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_optnumber(L,4,1),(float)luaL_optnumber(L,5,1),(float)luaL_optnumber(L,6,1),(float)luaL_optnumber(L,7,1));return 0;)
MG(add_point_light,lua_pushinteger(L,g_mg->addPointLight((float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_optnumber(L,4,1),(float)luaL_optnumber(L,5,1),(float)luaL_optnumber(L,6,1),(float)luaL_optnumber(L,7,1),(float)luaL_optnumber(L,8,10),luaL_optstring(L,9,"")));return 1;)
MG(remove_light,lua_pushboolean(L,g_mg->removeLight((uint32_t)luaL_checkinteger(L,1))?1:0);return 1;)
MG(check_collision,lua_pushboolean(L,g_mg->checkCollision((uint32_t)luaL_checkinteger(L,1),(uint32_t)luaL_checkinteger(L,2))?1:0);return 1;)
MG(set_collision,g_mg->setCollisionEnabled(lua_toboolean(L,1)!=0);return 0;)
MG(set_velocity,g_mg->setVelocity((uint32_t)luaL_checkinteger(L,1),(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4));return 0;)
MG(set_gravity,g_mg->setGravity((uint32_t)luaL_checkinteger(L,1),lua_toboolean(L,2)!=0);return 0;)
#undef MG

static const luaL_Reg mg_functions[]={
    {"spawn_cube",lua_mg_spawn_cube},{"spawn_sphere",lua_mg_spawn_sphere},{"spawn_plane",lua_mg_spawn_plane},
    {"remove_object",lua_mg_remove_object},{"set_camera",lua_mg_set_camera},
    {"create_material",lua_mg_create_material},{"set_material",lua_mg_set_material},
    {"set_ambient",lua_mg_set_ambient},{"set_directional",lua_mg_set_directional},
    {"add_point_light",lua_mg_add_point_light},{"remove_light",lua_mg_remove_light},
    {"check_collision",lua_mg_check_collision},{"set_collision",lua_mg_set_collision},
    {"set_velocity",lua_mg_set_velocity},{"set_gravity",lua_mg_set_gravity},
    {nullptr,nullptr}
};

void registerMiniGameBinding(lua_State* L,BgfxMiniGameBackend* backend){
    g_mg=backend; backend->setLuaState(L);
    luaL_newlib(L,mg_functions); lua_setglobal(L,"mini_game");
    printf("[MiniGame] Lua bindings ¡ª 15 functions\n");
}

} // namespace Caesura

