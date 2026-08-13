// SmaBinding.cpp — Lua `sma` global for the skeletal-mesh animation
// renderer (SMA Battle 4d S2/S3). Thin pass-through onto IMeshRenderer
// via BackendRegistry; scripts/kag/sma.lua owns data parsing, hierarchy
// resolution and animation driving.
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "SmaBinding.h"
#include "../../render/api/IMeshRenderer.h"
#include "../../di/BackendRegistry.h"
#include <cstring>

namespace Caesura {

// ===========================================================================
// Helpers
// ===========================================================================

static IMeshRenderer* getMeshRenderer() {
    return BackendRegistry::instance().getMeshRenderer();
}

// Read a SMAMesh from two Lua tables:
//   verts:   array of {x, y, u, v, bone0, w0, bone1?, w1?}
//   indices: array of numbers
static bool readMesh(lua_State* L, int vertIdx, int idxIdx, SMAMesh& out) {
    if (!lua_istable(L, vertIdx) || !lua_istable(L, idxIdx)) return false;
    const int vn = (int)lua_rawlen(L, vertIdx);
    const int in = (int)lua_rawlen(L, idxIdx);
    if (vn <= 0 || in <= 0 || in % 3 != 0) return false;

    out.vertices.resize((size_t)vn);
    for (int i = 1; i <= vn; ++i) {
        lua_rawgeti(L, vertIdx, i); // vert table
        if (!lua_istable(L, -1)) { lua_pop(L, 1); return false; }
        SMAMeshVertex& v = out.vertices[(size_t)(i - 1)];
        v.x = (float)luaL_optnumber(L, -1, 1);
        v.y = (float)luaL_optnumber(L, -1, 2);
        v.u = (float)luaL_optnumber(L, -1, 3);
        v.v = (float)luaL_optnumber(L, -1, 4);
        v.bone0 = (uint16_t)luaL_optinteger(L, -1, 5);
        v.w0 = (float)luaL_optnumber(L, -1, 6);
        v.bone1 = (uint16_t)luaL_optinteger(L, -1, 7);
        v.w1 = (float)luaL_optnumber(L, -1, 8);
        lua_pop(L, 1);
    }
    out.indices.resize((size_t)in);
    for (int i = 1; i <= in; ++i) {
        lua_rawgeti(L, idxIdx, i);
        out.indices[(size_t)(i - 1)] = (uint16_t)lua_tointeger(L, -1);
        lua_pop(L, 1);
    }
    return true;
}

// ===========================================================================
// sma.* functions
// ===========================================================================

static int lua_sma_create_mesh(lua_State* L) {
    IMeshRenderer* r = getMeshRenderer();
    SMAMesh mesh;
    if (!r || !readMesh(L, 1, 2, mesh)) {
        lua_pushinteger(L, 0);
        return 1;
    }
    const MeshHandle h = r->createMesh(mesh);
    lua_pushinteger(L, h ? (lua_Integer)h.id : 0);
    return 1;
}

static int lua_sma_destroy_mesh(lua_State* L) {
    IMeshRenderer* r = getMeshRenderer();
    if (r) r->destroyMesh(MeshHandle{ (uint32_t)luaL_optinteger(L, 1, 0) });
    return 0;
}

static int lua_sma_update_mesh(lua_State* L) {
    IMeshRenderer* r = getMeshRenderer();
    const MeshHandle h{ (uint32_t)luaL_optinteger(L, 1, 0) };
    if (!r || !lua_istable(L, 2)) return 0;
    const int n = (int)lua_rawlen(L, 2);
    std::vector<BonePose> poses;
    poses.resize((size_t)n);
    for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, 2, i); // pose table
        if (lua_istable(L, -1)) {
            BonePose& p = poses[(size_t)(i - 1)];
            p.rot = (float)luaL_optnumber(L, -1, 1);
            p.scale = (float)luaL_optnumber(L, -1, 2);
            p.ox = (float)luaL_optnumber(L, -1, 3);
            p.oy = (float)luaL_optnumber(L, -1, 4);
        }
        lua_pop(L, 1);
    }
    r->updateMesh(h, poses);
    return 0;
}

static int lua_sma_draw_mesh(lua_State* L) {
    IMeshRenderer* r = getMeshRenderer();
    if (!r) return 0;
    const MeshHandle h{ (uint32_t)luaL_optinteger(L, 1, 0) };
    const uint16_t view = (uint16_t)luaL_optinteger(L, 2, 0);
    const uint32_t texId = (uint32_t)luaL_optinteger(L, 3, 0);
    const float x = (float)luaL_optnumber(L, 4, 0.0);
    const float y = (float)luaL_optnumber(L, 5, 0.0);
    const float scale = (float)luaL_optnumber(L, 6, 1.0);
    const float opacity = (float)luaL_optnumber(L, 7, 1.0);
    r->drawMesh(view, h, texId, x, y, scale, opacity);
    return 0;
}

static int lua_sma_count(lua_State* L) {
    IMeshRenderer* r = getMeshRenderer();
    lua_pushinteger(L, r ? (lua_Integer)r->meshCount() : 0);
    return 1;
}

static int lua_sma_initialized(lua_State* L) {
    IMeshRenderer* r = getMeshRenderer();
    lua_pushboolean(L, r ? (r->isInitialized() ? 1 : 0) : 0);
    return 1;
}

// S5: skinning mode ("auto" | "cpu" | "gpu").
static int lua_sma_set_skin_mode(lua_State* L) {
    IMeshRenderer* r = getMeshRenderer();
    if (!r) return 0;
    const char* mode = luaL_optstring(L, 1, "auto");
    SkinMode m = SkinMode::Auto;
    if (std::strcmp(mode, "cpu") == 0) m = SkinMode::Cpu;
    else if (std::strcmp(mode, "gpu") == 0) m = SkinMode::Gpu;
    r->setSkinMode(m);
    return 0;
}

static int lua_sma_get_skin_mode(lua_State* L) {
    IMeshRenderer* r = getMeshRenderer();
    if (!r) { lua_pushstring(L, "cpu"); return 1; }
    switch (r->skinMode()) {
        case SkinMode::Cpu: lua_pushstring(L, "cpu"); break;
        case SkinMode::Gpu: lua_pushstring(L, "gpu"); break;
        default:            lua_pushstring(L, "auto"); break;
    }
    return 1;
}

// ===========================================================================

static const luaL_Reg sma_functions[] = {
    { "create_mesh",     lua_sma_create_mesh     },
    { "destroy_mesh",    lua_sma_destroy_mesh    },
    { "update_mesh",     lua_sma_update_mesh     },
    { "draw_mesh",       lua_sma_draw_mesh       },
    { "count",           lua_sma_count           },
    { "initialized",     lua_sma_initialized     },
    { "set_skin_mode",   lua_sma_set_skin_mode   },
    { "get_skin_mode",   lua_sma_get_skin_mode   },
    { nullptr, nullptr },
};

void registerSmaBinding(lua_State* L) {
    luaL_newlib(L, sma_functions);
    lua_setglobal(L, "sma");
}

} // namespace Caesura
