extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "RenderBinding.h"
#include "../Core/BackendRegistry.h"
#include "../Render/IRenderDevice.h"
#include "../Resource/AsyncLoader.h"
#include "../Render/TextureManager.h"
#include <bgfx/bgfx.h>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace Caesura {

// -- Internal texture helper: delegates to TextureManager -------------------

// Resolve a texture ID: first try TextureManager, then RTT viewport map.
// This handles the case where Lua passes an RTT view ID as "tex" field.
static bgfx::TextureHandle resolveTexture(uint32_t id, IRenderDevice* dev) {
    bgfx::TextureHandle tex = TextureManager::instance().getTextureHandle(id);
    if (!bgfx::isValid(tex) && dev && id != 0) {
        ViewportHandle vp{ id };
        tex = dev->getViewportTexture(vp);
    }
    return tex;
}

// Legacy wrapper for callers that don't have a device pointer
static bgfx::TextureHandle getTexHandle(uint32_t texId) {
    return TextureManager::instance().getTextureHandle(texId);
}

// -- Helpers ----------------------------------------------------------------

static IRenderDevice* getRender(lua_State* L) {
    return BackendRegistry::getRenderDeviceFromLua(L);
}

static int getTableInt(lua_State* L, const char* key, int def) {
    lua_getfield(L, -1, key);
    int v = lua_isnumber(L, -1) ? (int)lua_tointeger(L, -1) : def;
    lua_pop(L, 1);
    return v;
}

static float getTableFloat(lua_State* L, const char* key, float def) {
    lua_getfield(L, -1, key);
    float v = lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : def;
    lua_pop(L, 1);
    return v;
}

// -- Render.load_texture(file) ----------------------------------------------

static int lua_Render_load_texture(lua_State* L) {
    const char* file = luaL_checkstring(L, 1);

    uint32_t texId = TextureManager::instance().loadTexture(file);
    if (texId == 0) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to load texture");
        return 2;
    }
    lua_pushinteger(L, (lua_Integer)texId);
    return 1;
}

// -- Render.destroy_texture(texId) ------------------------------------------

static int lua_Render_destroy_texture(lua_State* L) {
    uint32_t texId = (uint32_t)luaL_checkinteger(L, 1);
    TextureManager::instance().destroyTexture(texId);
    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.create_solid_texture(r, g, b, a) --------------------------------

static int lua_Render_create_solid_texture(lua_State* L) {
    int r = (int)luaL_checkinteger(L, 1);
    int g = (int)luaL_checkinteger(L, 2);
    int b = (int)luaL_checkinteger(L, 3);
    int a = (int)luaL_optinteger(L, 4, 255);

    bgfx::TextureHandle tex = TextureManager::instance().createSolidTexture(
        (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
    if (!bgfx::isValid(tex)) {
        lua_pushnil(L); lua_pushstring(L, "GPU solid tex failed"); return 2;
    }

    uint32_t texId = TextureManager::instance().registerTexture(tex);
    printf("[Render] Solid texture RGBA(%d,%d,%d,%d) -> %u\n", r, g, b, a, texId);
    lua_pushinteger(L, (lua_Integer)texId);
    return 1;
}

// -- Render.get_resolution() ------------------------------------------------

static int lua_Render_get_resolution(lua_State* L) {
    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushinteger(L, 0); lua_pushinteger(L, 0); return 2; }
    lua_pushinteger(L, dev->getBackbufferWidth());
    lua_pushinteger(L, dev->getBackbufferHeight());
    return 2;
}

// -- Render.set_view_name(id, name) -----------------------------------------

static int lua_Render_set_view_name(lua_State* L) {
    uint16_t viewId = (uint16_t)luaL_checkinteger(L, 1);
    const char* name = luaL_checkstring(L, 2);
    IRenderDevice* dev = getRender(L);
    if (dev) dev->setDebugName(viewId, name);
    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.submit_batch(...) -- batch-submit layer quads -------------------

static int lua_Render_submit_batch(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);

    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }

    int n = (int)lua_rawlen(L, 1);
    if (n == 0) { lua_pushboolean(L, 1); return 1; }


    dev->beginBatch();

    for (int i = 1; i <= n; i++) {
        lua_rawgeti(L, 1, i);
        if (!lua_istable(L, -1)) { lua_pop(L, 1); continue; }
        uint32_t texId  = (uint32_t)getTableInt(L, "tex", 0);
        float    x      = getTableFloat(L, "x", 0);
        float    y      = getTableFloat(L, "y", 0);
        float    w      = getTableFloat(L, "w", 128);
        float    h      = getTableFloat(L, "h", 128);
        int      opacity = getTableInt(L, "opacity", 255);

        bgfx::TextureHandle tex = resolveTexture(texId, dev);
        // Also try explicit "rt" key for forward-compat
        if (!bgfx::isValid(tex)) {
            uint32_t rtId = (uint32_t)getTableInt(L, "rt", 0);
            if (rtId != 0) {
                tex = resolveTexture(rtId, dev);
            }
        }
        if (bgfx::isValid(tex)) {
            dev->blitTexture(VIEW_MAIN, (uint32_t)tex.idx, x, y, w, h, (uint8_t)opacity);
        }

        lua_pop(L, 1);
    }

    dev->flushBatch();
    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.submit_blend(baseTex, blendTex, mode, baseAlpha, blendAlpha, globalAlpha)

static int lua_Render_submit_blend(lua_State* L) {
    uint32_t baseTexId  = (uint32_t)luaL_checkinteger(L, 1);
    uint32_t blendTexId = (uint32_t)luaL_checkinteger(L, 2);
    int      mode       = (int)luaL_checkinteger(L, 3);
    float    baseAlpha  = (float)luaL_optnumber(L, 4, 1.0);
    float    blendAlpha = (float)luaL_optnumber(L, 5, 1.0);
    float    globalAlpha = (float)luaL_optnumber(L, 6, 1.0);

    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }
    bgfx::TextureHandle baseTex  = resolveTexture(baseTexId, dev);
    bgfx::TextureHandle blendTex = resolveTexture(blendTexId, dev);

    if (!bgfx::isValid(baseTex) || !bgfx::isValid(blendTex)) {
        fprintf(stderr, "[Render] submit_blend: invalid texture(s)\n");
        lua_pushboolean(L, 0); return 1;
    }

    dev->submitBlend(VIEW_MAIN, baseTex, blendTex,
                         mode, baseAlpha, blendAlpha, globalAlpha);
    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.submit_transition(fromTex, toTex, ruleTex, method, progress) ----

static int lua_Render_submit_transition(lua_State* L) {
    uint32_t fromTexId = (uint32_t)luaL_checkinteger(L, 1);
    uint32_t toTexId   = (uint32_t)luaL_checkinteger(L, 2);
    uint32_t ruleTexId = (uint32_t)luaL_optinteger(L, 3, 0);
    int      method    = (int)luaL_checkinteger(L, 4);
    float    progress  = (float)luaL_checknumber(L, 5);

    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }
    bgfx::TextureHandle fromTex = resolveTexture(fromTexId, dev);
    bgfx::TextureHandle toTex   = resolveTexture(toTexId, dev);

    if (!bgfx::isValid(fromTex) || !bgfx::isValid(toTex)) {
        fprintf(stderr, "[Render] submit_transition: invalid texture(s)\n");
        lua_pushboolean(L, 0); return 1;
    }

    bgfx::TextureHandle ruleTex = BGFX_INVALID_HANDLE;
    if (ruleTexId != 0) {
        ruleTex = resolveTexture(ruleTexId, dev);
    }

    dev->submitTransition(VIEW_MAIN, fromTex, toTex, ruleTex,
                               method, progress);
    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.submit_vfx(srcTex, effect, fadeAlpha, fadeR, fadeG, fadeB, blurRadius, quakeX, quakeY)

static int lua_Render_submit_vfx(lua_State* L) {
    uint32_t srcTexId  = (uint32_t)luaL_checkinteger(L, 1);
    int      effect    = (int)luaL_checkinteger(L, 2);
    float    fadeAlpha = (float)luaL_optnumber(L, 3, 1.0);
    float    fadeR     = (float)luaL_optnumber(L, 4, 0.0);
    float    fadeG     = (float)luaL_optnumber(L, 5, 0.0);
    float    fadeB     = (float)luaL_optnumber(L, 6, 0.0);
    float    blurRadius = (float)luaL_optnumber(L, 7, 0.0);
    float    quakeX    = (float)luaL_optnumber(L, 8, 0.0);
    float    quakeY    = (float)luaL_optnumber(L, 9, 0.0);

    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }
    bgfx::TextureHandle srcTex = resolveTexture(srcTexId, dev);
    if (!bgfx::isValid(srcTex)) {
        fprintf(stderr, "[Render] submit_vfx: invalid texture\n");
        lua_pushboolean(L, 0); return 1;
    }

    dev->submitVFX(VIEW_MAIN, srcTex, effect,
                       fadeAlpha, fadeR, fadeG, fadeB,
                       blurRadius, quakeX, quakeY);
    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.stretch_blt(dstTexId, dx,dy,dw,dh, srcTexId, sx,sy,sw,sh, filter) --

static int lua_Render_stretch_blt(lua_State* L) {
    uint32_t dstTexId = (uint32_t)luaL_checkinteger(L, 1);
    float    dx = (float)luaL_checknumber(L, 2);
    float    dy = (float)luaL_checknumber(L, 3);
    float    dw = (float)luaL_checknumber(L, 4);
    float    dh = (float)luaL_checknumber(L, 5);
    uint32_t srcTexId = (uint32_t)luaL_checkinteger(L, 6);
    float    sx = (float)luaL_checknumber(L, 7);
    float    sy = (float)luaL_checknumber(L, 8);
    float    sw = (float)luaL_checknumber(L, 9);
    float    sh = (float)luaL_checknumber(L, 10);
    int      filter = (int)luaL_optinteger(L, 11, 1);

    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }
    bgfx::TextureHandle srcTex = resolveTexture(srcTexId, dev);
    if (!bgfx::isValid(srcTex)) {
        lua_pushboolean(L, 0); return 1;
    }

    dev->stretchBlt(VIEW_MAIN, dstTexId,
                     dx, dy, dw, dh,
                     (uint32_t)srcTex.idx,
                     sx, sy, sw, sh,
                     filter);
    lua_pushboolean(L, 1); return 1;
}

// -- Render.affine_blt(dstTexId, dx,dy,dw,dh, srcTexId, sx,sy,sw,sh, matrix) --

static int lua_Render_affine_blt(lua_State* L) {
    uint32_t dstTexId = (uint32_t)luaL_checkinteger(L, 1);
    float    dx = (float)luaL_checknumber(L, 2);
    float    dy = (float)luaL_checknumber(L, 3);
    float    dw = (float)luaL_checknumber(L, 4);
    float    dh = (float)luaL_checknumber(L, 5);
    uint32_t srcTexId = (uint32_t)luaL_checkinteger(L, 6);
    float    sx = (float)luaL_checknumber(L, 7);
    float    sy = (float)luaL_checknumber(L, 8);
    float    sw = (float)luaL_checknumber(L, 9);
    float    sh = (float)luaL_checknumber(L, 10);

    float matrix[6] = { 1, 0, 0, 1, 0, 0 };

    if (lua_istable(L, 11)) {
        lua_getfield(L, 11, "a");  if (lua_isnumber(L, -1)) matrix[0] = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 11, "b");  if (lua_isnumber(L, -1)) matrix[1] = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 11, "c");  if (lua_isnumber(L, -1)) matrix[2] = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 11, "d");  if (lua_isnumber(L, -1)) matrix[3] = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 11, "tx"); if (lua_isnumber(L, -1)) matrix[4] = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 11, "ty"); if (lua_isnumber(L, -1)) matrix[5] = (float)lua_tonumber(L, -1); lua_pop(L, 1);

        if (!lua_isnumber(L, -1)) {
            for (int i = 0; i < 6; i++) {
                lua_rawgeti(L, 11, i + 1);
                if (lua_isnumber(L, -1)) matrix[i] = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);
            }
        }
    }

    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }
    bgfx::TextureHandle srcTex = resolveTexture(srcTexId, dev);
    if (!bgfx::isValid(srcTex)) {
        lua_pushboolean(L, 0); return 1;
    }

    dev->affineBlt(VIEW_MAIN, dstTexId,
                    dx, dy, dw, dh,
                    (uint32_t)srcTex.idx,
                    sx, sy, sw, sh,
                    matrix);
    lua_pushboolean(L, 1); return 1;
}

// -- Render.fill_viewport(vpHandle, r, g, b, a) ----------------------------

static int lua_Render_fill_viewport(lua_State* L) {
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }
    uint8_t r = (uint8_t)luaL_optinteger(L, 2, 0);
    uint8_t g = (uint8_t)luaL_optinteger(L, 3, 0);
    uint8_t b = (uint8_t)luaL_optinteger(L, 4, 0);
    uint8_t a = (uint8_t)luaL_optinteger(L, 5, 255);

    ViewportHandle vp{ id };
    dev->fillViewport(vp, r, g, b, a);
    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.create_viewport(w, h) -> handle ---------------------------------

static int lua_Render_create_viewport(lua_State* L) {
    int w = (int)luaL_checkinteger(L, 1);
    int h = (int)luaL_checkinteger(L, 2);

    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushinteger(L, 0); return 1; }

    ViewportHandle vp = dev->createRenderTarget(w, h);
    lua_pushinteger(L, (lua_Integer)vp.id);
    return 1;
}

// -- Render.destroy_viewport(handle) ----------------------------------------

static int lua_Render_destroy_viewport(lua_State* L) {
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    ViewportHandle vp{ id };

    IRenderDevice* dev = getRender(L);
    if (dev) dev->destroyRenderTarget(vp);
    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.draw_viewport(handle, x, y, w, h) -------------------------------

static int lua_Render_draw_viewport(lua_State* L) {
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    float x = (float)luaL_optnumber(L, 2, 0);
    float y = (float)luaL_optnumber(L, 3, 0);
    float w = (float)luaL_optnumber(L, 4, -1);
    float h = (float)luaL_optnumber(L, 5, -1);

    ViewportHandle vp{ id };
    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }

    if (w < 0) w = (float)dev->getBackbufferWidth();
    if (h < 0) h = (float)dev->getBackbufferHeight();

    dev->blitViewport(vp, VIEW_MAIN, x, y, w, h);
    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.resize(w, h) ----------------------------------------------------

static int lua_Render_resize(lua_State* L) {
    int w = (int)luaL_checkinteger(L, 1);
    int h = (int)luaL_checkinteger(L, 2);
    IRenderDevice* dev = getRender(L);
    if (dev) dev->resize(w, h);
    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.load_texture_async(path) ----------------------------------------

static int lua_Render_load_texture_async(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int id = AsyncLoader::instance().enqueue(path, "texture");
    lua_pushinteger(L, id);
    return 1;
}

// -- Render.cancel_async_loads() --------------------------------------------

static int lua_Render_cancel_async_loads(lua_State* L) {
    AsyncLoader::instance().cancelAll();
    lua_pushboolean(L, 1);
    return 1;
}

// -- Video placeholders -----------------------------------------------------

static int lua_Render_video_play(lua_State* L) {
    (void)luaL_checkstring(L, 1); // consume arg, validate type
    return luaL_error(L, "Video playback not available in Alpha (FFmpeg planned for Beta)");
}

static int lua_Render_video_stop(lua_State* L)     { lua_pushboolean(L, 1); return 1; }
static int lua_Render_video_update(lua_State* L)    { lua_pushboolean(L, 1); return 1; }
static int lua_Render_video_get_texture(lua_State* L) { lua_pushinteger(L, 0); return 1; }
static int lua_Render_video_is_playing(lua_State* L) { lua_pushboolean(L, 0); return 1; }
static int lua_Render_video_has_ended(lua_State* L)  { lua_pushboolean(L, 0); return 1; }
static int lua_Render_video_get_size(lua_State* L)   { lua_pushinteger(L, 0); lua_pushinteger(L, 0); return 2; }
static int lua_Render_video_pause(lua_State* L)      { lua_pushboolean(L, 1); return 1; }
static int lua_Render_video_resume(lua_State* L)     { lua_pushboolean(L, 1); return 1; }

// -- ResourceHandle validation (Phase 0.5) ---------------------------------

static int lua_Render_is_valid_handle(lua_State* L) {
    int typeInt = (int)luaL_checkinteger(L, 1);
    uint32_t id = (uint32_t)luaL_checkinteger(L, 2);
    if (typeInt < 0 || typeInt > 7) { lua_pushboolean(L, 0); return 1; }

    HandleType type = static_cast<HandleType>(typeInt);
    if (id == 0) { lua_pushboolean(L, 0); return 1; }

    switch (type) {
        case HandleType::TEXTURE: {
            bool valid = TextureManager::instance().isValid(id);
            lua_pushboolean(L, valid ? 1 : 0);
            return 1;
        }
        case HandleType::VIEWPORT:
        case HandleType::RTT: {
            lua_pushboolean(L, 1);
            return 1;
        }
        default:
            lua_pushboolean(L, id != 0 ? 1 : 0);
            return 1;
    }
}

static int lua_Render_invalidate_handles(lua_State* L) {
    int typeInt = (int)luaL_checkinteger(L, 1);
    if (typeInt < 0 || typeInt > 7) { lua_pushboolean(L, 0); return 1; }
    HandleType type = static_cast<HandleType>(typeInt);
    BackendRegistry::instance().invalidateHandles(type);
    printf("[Render] Handles invalidated: %s\n", handleTypeName(type));
    lua_pushboolean(L, 1);
    return 1;
}

// -- Module registration ----------------------------------------------------

static const luaL_Reg render_functions[] = {
    { "create_viewport",    lua_Render_create_viewport    },
    { "destroy_viewport",   lua_Render_destroy_viewport   },
    { "draw_viewport",      lua_Render_draw_viewport      },
    { "load_texture",       lua_Render_load_texture       },
    { "destroy_texture",    lua_Render_destroy_texture    },
    { "create_solid_texture", lua_Render_create_solid_texture },
    { "get_resolution",     lua_Render_get_resolution     },
    { "set_view_name",      lua_Render_set_view_name      },
    { "submit_batch",       lua_Render_submit_batch       },
    { "submit_blend",       lua_Render_submit_blend       },
    { "submit_transition",  lua_Render_submit_transition  },
    { "submit_vfx",         lua_Render_submit_vfx         },
    { "stretch_blt",        lua_Render_stretch_blt        },
    { "affine_blt",         lua_Render_affine_blt         },
    { "fill_viewport",      lua_Render_fill_viewport      },
    { "resize",             lua_Render_resize             },
    { "is_valid_handle",    lua_Render_is_valid_handle   },
    { "invalidate_handles", lua_Render_invalidate_handles },
    { "load_texture_async",  lua_Render_load_texture_async  },
    { "cancel_async_loads",  lua_Render_cancel_async_loads  },
    { "video_stop",         lua_Render_video_stop         },
    { "video_update",       lua_Render_video_update       },
    { "video_get_texture",  lua_Render_video_get_texture  },
    { "video_is_playing",   lua_Render_video_is_playing   },
    { "video_has_ended",    lua_Render_video_has_ended    },
    { "video_get_size",     lua_Render_video_get_size     },
    { "video_pause",        lua_Render_video_pause        },
    { "video_resume",       lua_Render_video_resume       },
    { nullptr, nullptr }
};

void registerRenderBinding(lua_State* L) {
    luaL_newlib(L, render_functions);
    lua_setglobal(L, "Render");
    printf("[Lua] Render module registered (via BackendRegistry).\n");
}

} // namespace Caesura

