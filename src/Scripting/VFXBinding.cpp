extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "VFXBinding.h"
#include "../Core/BackendRegistry.h"
#include "../Render/IRenderDevice.h"
#include "../Render/BgfxRenderDevice.h"
#include "../Render/ParticleSystem.h"
#include "../Core/BackendRegistry.h"
#include <cstdio>

namespace Caesura {

// ===========================================================================
// Singleton ParticleSystem instance -- managed by VFXBinding lifecycle
// ===========================================================================

static ParticleSystem s_particleSystem;
static bool s_particlesInitialized = false;

// ===========================================================================
// Helper: get BgfxRenderDevice from Lua/BackendRegistry
// ===========================================================================

static BgfxRenderDevice* getBgfxDevice(lua_State* L) {
    IRenderDevice* dev = BackendRegistry::getRenderDeviceFromLua(L);
    if (!dev) return nullptr;
    return dynamic_cast<BgfxRenderDevice*>(dev);
}

// ===========================================================================
// Lua binding functions
// ===========================================================================

// -- VFX.particles_init() ----------------------------------------------------

static int lua_VFX_particles_init(lua_State* L) {
    if (s_particlesInitialized) {
        lua_pushboolean(L, 1);
        return 1;
    }

    BgfxRenderDevice* device = getBgfxDevice(L);
    if (!device) {
        lua_pushnil(L);
        lua_pushstring(L, "BgfxRenderDevice not available for particle init");
        return 2;
    }

    bool ok = s_particleSystem.init(device);
    s_particlesInitialized = ok;
    if (ok) {
        printf("[VFX] Particle system initialized.\n");
    } else {
        printf("[VFX] Particle system init failed!\n");
    }
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// -- VFX.particles_shutdown() ------------------------------------------------

static int lua_VFX_particles_shutdown(lua_State* L) {
    s_particleSystem.shutdown();
    s_particlesInitialized = false;
    printf("[VFX] Particle system shut down.\n");
    lua_pushboolean(L, 1);
    return 1;
}

// -- VFX.particles_create_emitter(cfg_table) ---------------------------------

static int lua_VFX_particles_create_emitter(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);

    Emitter cfg;
    lua_getfield(L, 1, "x");         cfg.x       = lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : 0.0f;  lua_pop(L, 1);
    lua_getfield(L, 1, "y");         cfg.y       = lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : 0.0f;  lua_pop(L, 1);
    lua_getfield(L, 1, "rate");      cfg.rate    = lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : 10.0f; lua_pop(L, 1);
    lua_getfield(L, 1, "lifeMin");   cfg.lifeMin = lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : 0.5f;  lua_pop(L, 1);
    lua_getfield(L, 1, "lifeMax");   cfg.lifeMax = lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : 2.0f;  lua_pop(L, 1);
    lua_getfield(L, 1, "speedMin");  cfg.speedMin= lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : 10.0f; lua_pop(L, 1);
    lua_getfield(L, 1, "speedMax");  cfg.speedMax= lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : 50.0f; lua_pop(L, 1);
    lua_getfield(L, 1, "angleMin");  cfg.angleMin=lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : 0.0f;  lua_pop(L, 1);
    lua_getfield(L, 1, "angleMax");  cfg.angleMax=lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : 6.283f;lua_pop(L, 1);
    lua_getfield(L, 1, "sizeMin");   cfg.sizeMin =lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : 2.0f;  lua_pop(L, 1);
    lua_getfield(L, 1, "sizeMax");   cfg.sizeMax =lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : 8.0f;  lua_pop(L, 1);
    lua_getfield(L, 1, "r");         cfg.r       =lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : 1.0f;  lua_pop(L, 1);
    lua_getfield(L, 1, "g");         cfg.g       =lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : 1.0f;  lua_pop(L, 1);
    lua_getfield(L, 1, "b");         cfg.b       =lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : 1.0f;  lua_pop(L, 1);
    lua_getfield(L, 1, "a");         cfg.a       =lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : 1.0f;  lua_pop(L, 1);
    lua_getfield(L, 1, "gravityX");  cfg.gravityX=lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : 0.0f;  lua_pop(L, 1);
    lua_getfield(L, 1, "gravityY");  cfg.gravityY=lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : 0.0f;  lua_pop(L, 1);

    int id = s_particleSystem.createEmitter(cfg);
    lua_pushinteger(L, id);
    return 1;
}

// -- VFX.particles_destroy_emitter(id) ---------------------------------------

static int lua_VFX_particles_destroy_emitter(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    s_particleSystem.destroyEmitter(id);
    lua_pushboolean(L, 1);
    return 1;
}

// -- VFX.particles_emit(emitter_id, count) -----------------------------------

static int lua_VFX_particles_emit(lua_State* L) {
    int emitterId = (int)luaL_checkinteger(L, 1);
    int count     = (int)luaL_checkinteger(L, 2);
    s_particleSystem.emit(emitterId, count);
    lua_pushboolean(L, 1);
    return 1;
}

// -- VFX.particles_update(dt) -------------------------------------------------

static int lua_VFX_particles_update(lua_State* L) {
    float dt = (float)luaL_checknumber(L, 1);
    int sw = 1280, sh = 720;
    auto* rd = BackendRegistry::instance().getRenderDevice();
    if (rd) { sw = rd->getBackbufferWidth(); sh = rd->getBackbufferHeight(); }
    s_particleSystem.update(dt, (uint32_t)sw, (uint32_t)sh);
    lua_pushboolean(L, 1);
    return 1;
}

// -- VFX.particles_render(view_id) --------------------------------------------

static int lua_VFX_particles_render(lua_State* L) {
    uint16_t viewId = (uint16_t)luaL_checkinteger(L, 1);
    s_particleSystem.render(viewId);
    lua_pushboolean(L, 1);
    return 1;
}

// -- VFX.particles_alive_count() ----------------------------------------------

static int lua_VFX_particles_alive_count(lua_State* L) {
    lua_pushinteger(L, s_particleSystem.aliveCount());
    return 1;
}

// -- VFX.particles_is_initialized() -------------------------------------------

static int lua_VFX_particles_is_initialized(lua_State* L) {
    lua_pushboolean(L, s_particlesInitialized ? 1 : 0);
    return 1;
}

// -- VFX.particles_clear() -- reset all emitters and particles ----------------

static int lua_VFX_particles_clear(lua_State* L) {
    s_particleSystem.shutdown();
    s_particlesInitialized = false;

    BgfxRenderDevice* device = getBgfxDevice(L);
    if (device) {
        s_particleSystem.init(device);
        s_particlesInitialized = true;
    }
    lua_pushboolean(L, s_particlesInitialized ? 1 : 0);
    return 1;
}

// ===========================================================================
// Module registration
// ===========================================================================

static const luaL_Reg vfx_functions[] = {
    { "particles_init",             lua_VFX_particles_init             },
    { "particles_shutdown",         lua_VFX_particles_shutdown         },
    { "particles_create_emitter",   lua_VFX_particles_create_emitter   },
    { "particles_destroy_emitter",  lua_VFX_particles_destroy_emitter  },
    { "particles_emit",             lua_VFX_particles_emit             },
    { "particles_update",           lua_VFX_particles_update           },
    { "particles_render",           lua_VFX_particles_render           },
    { "particles_alive_count",      lua_VFX_particles_alive_count      },
    { "particles_is_initialized",   lua_VFX_particles_is_initialized   },
    { "particles_clear",            lua_VFX_particles_clear            },
    { nullptr, nullptr }
};

void registerVFXBinding(lua_State* L) {
    luaL_newlib(L, vfx_functions);
    lua_setglobal(L, "VFX");
    printf("[Lua] VFX module registered (10 particle APIs).\n");
}

void VFXBinding_Shutdown() {
    if (s_particlesInitialized) {
        s_particleSystem.shutdown();
        s_particlesInitialized = false;
        printf("[VFX] Particle system shut down (VFXBinding_Shutdown).\n");
    }
}

} // namespace Caesura