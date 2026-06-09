#include "BgfxMiniGameBackend.h"
#include "../Core/BackendRegistry.h"
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <cstdio>
#include <cstring>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace Caesura {

// ==========================================================================
// Cube geometry: 24 unique vertices (position + normal), 36 indices
// ==========================================================================
struct PosNormVertex { float x, y, z; float nx, ny, nz; };

static const PosNormVertex kCubeVertices[] = {
    // +X face
    { 0.5, -0.5,  0.5,  1, 0, 0 }, { 0.5,  0.5,  0.5,  1, 0, 0 },
    { 0.5,  0.5, -0.5,  1, 0, 0 }, { 0.5, -0.5, -0.5,  1, 0, 0 },
    // -X face
    {-0.5, -0.5, -0.5, -1, 0, 0 }, {-0.5,  0.5, -0.5, -1, 0, 0 },
    {-0.5,  0.5,  0.5, -1, 0, 0 }, {-0.5, -0.5,  0.5, -1, 0, 0 },
    // +Y face
    {-0.5,  0.5,  0.5,  0, 1, 0 }, { 0.5,  0.5,  0.5,  0, 1, 0 },
    { 0.5,  0.5, -0.5,  0, 1, 0 }, {-0.5,  0.5, -0.5,  0, 1, 0 },
    // -Y face
    {-0.5, -0.5, -0.5,  0,-1, 0 }, { 0.5, -0.5, -0.5,  0,-1, 0 },
    { 0.5, -0.5,  0.5,  0,-1, 0 }, {-0.5, -0.5,  0.5,  0,-1, 0 },
    // +Z face
    {-0.5, -0.5,  0.5,  0, 0, 1 }, { 0.5, -0.5,  0.5,  0, 0, 1 },
    { 0.5,  0.5,  0.5,  0, 0, 1 }, {-0.5,  0.5,  0.5,  0, 0, 1 },
    // -Z face
    { 0.5, -0.5, -0.5,  0, 0,-1 }, {-0.5, -0.5, -0.5,  0, 0,-1 },
    {-0.5,  0.5, -0.5,  0, 0,-1 }, { 0.5,  0.5, -0.5,  0, 0,-1 },
};

static const uint16_t kCubeIndices[] = {
     0, 1, 2,  0, 2, 3,   4, 5, 6,  4, 6, 7,
     8, 9,10,  8,10,11,  12,13,14, 12,14,15,
    16,17,18, 16,18,19,  20,21,22, 20,22,23,
};

// ==========================================================================
// Lifecycle
// ==========================================================================

BgfxMiniGameBackend::~BgfxMiniGameBackend() { shutdown(); }

bool BgfxMiniGameBackend::init() {
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal,    3, bgfx::AttribType::Float)
        .end();

    m_layout = layout;

    // Create cube VB + IB
    const bgfx::Memory* vbMem = bgfx::copy(kCubeVertices, sizeof(kCubeVertices));
    m_cubeVB = bgfx::createVertexBuffer(vbMem, layout);

    const bgfx::Memory* ibMem = bgfx::copy(kCubeIndices, sizeof(kCubeIndices));
    m_cubeIB = bgfx::createIndexBuffer(ibMem);

    // Uniforms
    m_u_mtx      = bgfx::createUniform("u_mtx",      bgfx::UniformType::Mat4);
    m_u_color    = bgfx::createUniform("u_color",    bgfx::UniformType::Vec4);
    m_u_lightDir = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);

    // Load shaders — attempt from RenderDevice shared programs
    if (m_renderDevice) {
        bgfx::ProgramHandle prog = m_renderDevice->getFallbackProgram();
        if (bgfx::isValid(prog)) {
            m_program = prog;
        }
    }

    // If no shared program, try embedded
    if (!bgfx::isValid(m_program)) {
        // Fallback: use bgfx debug wireframe for cubes
        bgfx::setDebug(BGFX_DEBUG_WIREFRAME);
        printf("[MiniGame] 3D shader not loaded — using debug wireframe\n");
    }

    printf("[MiniGame] Bgfx 3D backend initialized\n");
    return true;
}

void BgfxMiniGameBackend::shutdown() {
    if (bgfx::isValid(m_cubeVB))    bgfx::destroy(m_cubeVB);
    if (bgfx::isValid(m_cubeIB))    bgfx::destroy(m_cubeIB);
    if (bgfx::isValid(m_u_mtx))     bgfx::destroy(m_u_mtx);
    if (bgfx::isValid(m_u_color))   bgfx::destroy(m_u_color);
    if (bgfx::isValid(m_u_lightDir)) bgfx::destroy(m_u_lightDir);
    m_objects.clear();
    m_active = false;
    printf("[MiniGame] Bgfx 3D backend shutdown\n");
}

// ==========================================================================
// Scene management
// ==========================================================================

uint32_t BgfxMiniGameBackend::loadScene(const std::string& path) {
    printf("[MiniGame] loadScene: %s → scene %u\n", path.c_str(), m_nextSceneId);
    return m_nextSceneId++;
}

void BgfxMiniGameBackend::unloadScene(uint32_t sceneHandle) {
    printf("[MiniGame] unloadScene: %u\n", sceneHandle);
    if (sceneHandle == m_activeScene) leave();
}

void BgfxMiniGameBackend::enter(uint32_t sceneHandle) {
    m_activeScene = sceneHandle;
    m_active = true;
    printf("[MiniGame] Enter 3D scene %u\n", sceneHandle);
}

void BgfxMiniGameBackend::leave() {
    m_active = false;
    printf("[MiniGame] Leave 3D scene\n");
}

// ==========================================================================
// Game loop
// ==========================================================================

bool BgfxMiniGameBackend::update(float deltaTime) {
    (void)deltaTime;
    return true;  // keep running
}

void BgfxMiniGameBackend::render() {
    if (!m_active) return;

    // Set view transform for mini-game view
    float view[16], proj[16];
    bx::mtxLookAt(view, bx::Vec3{m_camera.eyeX, m_camera.eyeY, m_camera.eyeZ},
                       bx::Vec3{m_camera.atX, m_camera.atY, m_camera.atZ});
    bx::mtxProj(proj, m_camera.fov, 1280.0f / 720.0f,
                m_camera.nearPlane, m_camera.farPlane, bgfx::getCaps()->homogeneousDepth);

    bgfx::setViewTransform(MINIGAME_VIEW, view, proj);
    bgfx::setViewRect(MINIGAME_VIEW, 0, 0, 1280, 720);
    bgfx::setViewClear(MINIGAME_VIEW, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

    // Submit all objects
    for (const auto& [id, obj] : m_objects) {
        submitObject(obj);
    }

    bgfx::touch(MINIGAME_VIEW);
}

void BgfxMiniGameBackend::submitObject(const MiniObject& obj) {
    float mtx[16];
    bx::mtxSRT(mtx, obj.scaleX, obj.scaleY, obj.scaleZ,
               obj.rotX, obj.rotY, obj.rotZ,
               obj.posX, obj.posY, obj.posZ);

    bgfx::setTransform(mtx);
    bgfx::setVertexBuffer(0, m_cubeVB);
    bgfx::setIndexBuffer(m_cubeIB);

    float color[4] = { obj.r, obj.g, obj.b, 1.0f };
    float lightDir[4] = { 0.577f, 0.577f, 0.577f, 0.0f };  // normalized
    bgfx::setUniform(m_u_color, color);
    bgfx::setUniform(m_u_lightDir, lightDir);

    if (bgfx::isValid(m_program)) {
        bgfx::submit(MINIGAME_VIEW, m_program);
    } else {
        // Debug primitives as fallback
        bgfx::dbgTextClear();
        bgfx::dbgTextPrintf(1, 1, 0x0f, "[MiniGame] 3D scene active — %zu objects", m_objects.size());
        bgfx::setDebug(BGFX_DEBUG_WIREFRAME);
        bgfx::submit(MINIGAME_VIEW, BGFX_INVALID_HANDLE);
    }
}

// ==========================================================================
// Input
// ==========================================================================

bool BgfxMiniGameBackend::processEvent(const void* sdlEvent) {
    (void)sdlEvent;
    return false;
}

// ==========================================================================
// Lua bridge dispatch
// ==========================================================================

int BgfxMiniGameBackend::luaCall(lua_State* L, const char* method) {
    if (strcmp(method, "spawn_cube") == 0) {
        float x = (float)luaL_checknumber(L, 2);
        float y = (float)luaL_checknumber(L, 3);
        float z = (float)luaL_checknumber(L, 4);
        float s = (float)luaL_optnumber(L, 5, 1.0);
        float r = (float)luaL_optnumber(L, 6, 1.0);
        float g = (float)luaL_optnumber(L, 7, 1.0);
        float b = (float)luaL_optnumber(L, 8, 1.0);
        uint32_t id = spawnCube(x, y, z, s, r, g, b);
        lua_pushinteger(L, id);
        return 1;
    }
    if (strcmp(method, "remove_object") == 0) {
        removeObject((uint32_t)luaL_checkinteger(L, 2));
        lua_pushboolean(L, 1);
        return 1;
    }
    if (strcmp(method, "set_camera") == 0) {
        setCamera((float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3),
                  (float)luaL_checknumber(L, 4), (float)luaL_checknumber(L, 5),
                  (float)luaL_checknumber(L, 6), (float)luaL_checknumber(L, 7));
        lua_pushboolean(L, 1);
        return 1;
    }
    if (strcmp(method, "set_light") == 0) {
        setAmbientLight((float)luaL_checknumber(L, 2),
                        (float)luaL_checknumber(L, 3),
                        (float)luaL_checknumber(L, 4));
        lua_pushboolean(L, 1);
        return 1;
    }
    printf("[MiniGame] Unknown luaCall method: %s\n", method);
    lua_pushboolean(L, 0);
    return 1;
}

// ==========================================================================
// Object API
// ==========================================================================

uint32_t BgfxMiniGameBackend::spawnCube(float x, float y, float z, float size,
                                         float r, float g, float b) {
    uint32_t id = m_nextObjId++;
    MiniObject obj;
    obj.id = id;
    obj.posX = x; obj.posY = y; obj.posZ = z;
    obj.scaleX = size; obj.scaleY = size; obj.scaleZ = size;
    obj.r = r; obj.g = g; obj.b = b;
    m_objects[id] = obj;
    printf("[MiniGame] Spawn cube #%u at (%.1f, %.1f, %.1f)\n", id, x, y, z);
    return id;
}

void BgfxMiniGameBackend::removeObject(uint32_t id) {
    m_objects.erase(id);
}

void BgfxMiniGameBackend::setCamera(float eyeX, float eyeY, float eyeZ,
                                     float atX, float atY, float atZ) {
    m_camera.eyeX = eyeX; m_camera.eyeY = eyeY; m_camera.eyeZ = eyeZ;
    m_camera.atX  = atX;  m_camera.atY  = atY;  m_camera.atZ  = atZ;
}

void BgfxMiniGameBackend::setAmbientLight(float r, float g, float b) {
    // Store for future use — currently uses hardcoded directional light
    (void)r; (void)g; (void)b;
    printf("[MiniGame] Light set to (%.2f, %.2f, %.2f)\n", r, g, b);
}

// ==========================================================================
// Lua binding registration
// ==========================================================================

static BgfxMiniGameBackend* g_miniGame = nullptr;

static int lua_mg_spawn_cube(lua_State* L) {
    if (!g_miniGame) { lua_pushinteger(L, 0); return 1; }
    uint32_t id = g_miniGame->spawnCube(
        (float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2),
        (float)luaL_checknumber(L, 3), (float)luaL_optnumber(L, 4, 1.0),
        (float)luaL_optnumber(L, 5, 1.0), (float)luaL_optnumber(L, 6, 1.0),
        (float)luaL_optnumber(L, 7, 1.0));
    lua_pushinteger(L, id);
    return 1;
}

static int lua_mg_remove_object(lua_State* L) {
    if (g_miniGame) g_miniGame->removeObject((uint32_t)luaL_checkinteger(L, 1));
    return 0;
}

static int lua_mg_set_camera(lua_State* L) {
    if (g_miniGame) g_miniGame->setCamera(
        (float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2),
        (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4),
        (float)luaL_checknumber(L, 5), (float)luaL_checknumber(L, 6));
    return 0;
}

static const luaL_Reg mg_functions[] = {
    { "spawn_cube",    lua_mg_spawn_cube    },
    { "remove_object", lua_mg_remove_object },
    { "set_camera",    lua_mg_set_camera    },
    { nullptr, nullptr }
};

void registerMiniGameBinding(lua_State* L, BgfxMiniGameBackend* backend) {
    g_miniGame = backend;
    luaL_newlib(L, mg_functions);
    lua_setglobal(L, "mini_game");
    printf("[MiniGame] Lua bindings registered (mini_game.*)\n");
}

} // namespace Caesura