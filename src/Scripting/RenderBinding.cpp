extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "RenderBinding.h"
#include "../Core/BackendRegistry.h"
#include "../Render/IRenderDevice.h"
#include "../Render/BgfxRenderDevice.h"
#include <bgfx/bgfx.h>
#include <bimg/decode.h>
#include <bx/file.h>
#include <bx/allocator.h>
#include <cstdio>
#include <cstring>
#define STB_IMAGE_IMPLEMENTATION
#include "../../external/stb/stb_image.h"
#include <unordered_map>
#include <vector>

namespace Caesura {

// -- Internal texture cache -------------------------------------------------

static bx::DefaultAllocator g_bxAllocator;
static std::unordered_map<uint32_t, bgfx::TextureHandle> g_textureCache;
static uint32_t g_nextTextureId = 1;

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
    // Security: reject path traversal via ..
    if (file && strstr(file, "..")) {
        fprintf(stderr, "[Render] load_texture: path traversal blocked: %s\n", file);
        lua_pushnil(L); lua_pushstring(L, "Path traversal blocked"); return 2;
    }

    if (!file || file[0] == 0) {
        lua_pushnil(L); lua_pushstring(L, "Invalid filename"); return 2;
    }

    bx::FileReader reader;
    if (!bx::open(&reader, file)) {
        fprintf(stderr, "[Render] load_texture: not found: %s\n", file);
        lua_pushnil(L); lua_pushstring(L, "File not found"); return 2;
    }

    uint32_t size = (uint32_t)bx::getSize(&reader);
    std::vector<uint8_t> buf(size);
    bx::read(&reader, buf.data(), size, bx::ErrorAssert{});
    bx::close(&reader);

    bimg::ImageContainer* img = bimg::imageParse(&g_bxAllocator, buf.data(), size);

    if (!img) {
        // Fallback: try stb_image
        int iw, ih, channels;
        unsigned char* stbData = stbi_load_from_memory(buf.data(), (int)size, &iw, &ih, &channels, 4);
        if (!stbData) {
            fprintf(stderr, "[Render] load_texture: decode failed (bimg+stb): %s\n", file);
            lua_pushnil(L); lua_pushstring(L, "Decode failed"); return 2;
        }
        const bgfx::Memory* mem = bgfx::copy(stbData, (uint32_t)(iw * ih * 4));
        stbi_image_free(stbData);
        bgfx::TextureHandle tex = bgfx::createTexture2D(
            (uint16_t)iw, (uint16_t)ih, false, 1,
            bgfx::TextureFormat::RGBA8,
            BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, mem);
        if (!bgfx::isValid(tex)) {
            lua_pushnil(L); lua_pushstring(L, "GPU tex failed"); return 2;
        }
        uint32_t texId = g_nextTextureId++;
        g_textureCache[texId] = tex;
        printf("[Render] Texture (stb): %s (%dx%d) -> %u\n", file, iw, ih, texId);
        lua_pushinteger(L, (lua_Integer)texId);
        return 1;
    }

    buf.clear();
    bgfx::TextureFormat::Enum fmt = bgfx::TextureFormat::RGBA8;
    if (img->m_format == bimg::TextureFormat::BGRA8) fmt = bgfx::TextureFormat::BGRA8;
    else if (img->m_format == bimg::TextureFormat::RGB8) fmt = bgfx::TextureFormat::RGB8;

    const bgfx::Memory* mem = bgfx::makeRef(img->m_data, img->m_size,
        [](void*, void* ud) { bimg::imageFree((bimg::ImageContainer*)ud); }, img);

    bgfx::TextureHandle tex = bgfx::createTexture2D(
        uint16_t(img->m_width), uint16_t(img->m_height),
        false, 1, fmt, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, mem);

    if (!bgfx::isValid(tex)) {
        bimg::imageFree(img);
        lua_pushnil(L); lua_pushstring(L, "GPU tex failed"); return 2;
    }

    uint32_t texId = g_nextTextureId++;
    g_textureCache[texId] = tex;
    printf("[Render] Texture: %s (%dx%d) -> %u\n", file, img->m_width, img->m_height, texId);
    lua_pushinteger(L, (lua_Integer)texId);
    return 1;
}

// -- Render.destroy_texture(texId) ------------------------------------------

static int lua_Render_destroy_texture(lua_State* L) {
    uint32_t texId = (uint32_t)luaL_checkinteger(L, 1);

    auto it = g_textureCache.find(texId);
    if (it != g_textureCache.end()) {
        if (bgfx::isValid(it->second)) {
            bgfx::destroy(it->second);
        }
        g_textureCache.erase(it);
        printf("[Render] Texture %u destroyed\n", texId);
    }

    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.create_viewport(w, h) -------------------------------------------

static int lua_Render_create_viewport(lua_State* L) {
    int width  = (int)luaL_checkinteger(L, 1);
    int height = (int)luaL_checkinteger(L, 2);

    IRenderDevice* dev = getRender(L);
    if (!dev) {
        lua_pushnil(L); lua_pushstring(L, "Render device not initialized"); return 2;
    }

    ViewportHandle handle = dev->createRenderTarget(width, height);
    if (!handle) {
        lua_pushnil(L); lua_pushstring(L, "Failed to create viewport"); return 2;
    }

    lua_pushinteger(L, (lua_Integer)handle.id);
    return 1;
}

// -- Render.destroy_viewport(handleId) --------------------------------------

static int lua_Render_destroy_viewport(lua_State* L) {
    uint32_t handleId = (uint32_t)luaL_checkinteger(L, 1);

    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }

    dev->destroyRenderTarget(ViewportHandle{handleId});
    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.draw_viewport(handleId, x, y, w, h) -----------------------------

static int lua_Render_draw_viewport(lua_State* L) {
    uint32_t handleId = (uint32_t)luaL_checkinteger(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float w = (float)luaL_checknumber(L, 4);
    float h = (float)luaL_checknumber(L, 5);

    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }

    dev->blitViewport(ViewportHandle{handleId}, VIEW_MAIN, x, y, w, h);
    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.get_resolution() ------------------------------------------------

// -- Render.create_solid_texture(r, g, b, a) ---------------------------
static int lua_Render_create_solid_texture(lua_State* L) {
    uint8_t r = (uint8_t)luaL_optinteger(L, 1, 255);
    uint8_t g = (uint8_t)luaL_optinteger(L, 2, 255);
    uint8_t b = (uint8_t)luaL_optinteger(L, 3, 255);
    uint8_t a = (uint8_t)luaL_optinteger(L, 4, 255);
    uint8_t pixel[4] = { r, g, b, a };
    const bgfx::Memory* mem = bgfx::copy(pixel, 4);
    bgfx::TextureHandle tex = bgfx::createTexture2D(1, 1, false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, mem);
    if (!bgfx::isValid(tex)) {
        lua_pushnil(L); lua_pushstring(L, "Failed"); return 2;
    }
    uint32_t texId = g_nextTextureId++;
    g_textureCache[texId] = tex;
    fprintf(stderr, "[Render] Solid texture #%u (RGBA %d,%d,%d,%d)\n", texId, r, g, b, a);
    lua_pushinteger(L, (lua_Integer)texId);
    return 1;
}


static int lua_Render_get_resolution(lua_State* L) {
    IRenderDevice* dev = getRender(L);
    if (!dev) {
        lua_pushinteger(L, 0); lua_pushinteger(L, 0);
        return 2;
    }

    lua_pushinteger(L, dev->getBackbufferWidth());
    lua_pushinteger(L, dev->getBackbufferHeight());
    return 2;
}

// -- Render.set_view_name(viewId, name) -------------------------------------

static int lua_Render_set_view_name(lua_State* L) {
    uint16_t viewId = (uint16_t)luaL_checkinteger(L, 1);
    const char* name = luaL_checkstring(L, 2);

    IRenderDevice* dev = getRender(L);
    if (dev) dev->setDebugName(viewId, name);
    return 0;
}

// -- Render.submit_batch(batch) ---------------------------------------------

static int lua_Render_submit_batch(lua_State* L) {
    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }

    luaL_checktype(L, 1, LUA_TTABLE);

    lua_getfield(L, 1, "commands");
    int topForCommands = lua_gettop(L);
    bool hasCommandsWrapper = lua_istable(L, -1);

    lua_Integer cmdCount = 0;
    int cmdIdx = 0;

    if (hasCommandsWrapper) {
        cmdCount = lua_rawlen(L, -1);
        cmdIdx = topForCommands;
    } else {
        lua_pop(L, 1);
        cmdCount = lua_rawlen(L, 1);
        cmdIdx = 1;
    }

    for (lua_Integer i = 1; i <= cmdCount; i++) {
        lua_rawgeti(L, cmdIdx, (int)i);
        if (!lua_istable(L, -1)) { lua_pop(L, 1); continue; }

        int texId       = getTableInt(L, "texture", getTableInt(L, "tex", 0));
        float cx        = getTableFloat(L, "x", 0.0f);
        float cy        = getTableFloat(L, "y", 0.0f);
        float cw        = getTableFloat(L, "w", 1280.0f);
        float ch        = getTableFloat(L, "h", 720.0f);
        float originX   = getTableFloat(L, "originX", 0.0f);
        float originY   = getTableFloat(L, "originY", 0.0f);
        float alpha     = getTableFloat(L, "alpha", getTableFloat(L, "opacity", 1.0f));
        uint8_t opacity = (uint8_t)(alpha * 255.0f);
        int viewId      = getTableInt(L, "view", VIEW_MAIN);

        lua_pop(L, 1);

        if (texId > 0) { /* printf throttled by layer visibility log */
            auto it = g_textureCache.find((uint32_t)texId);
            if (it != g_textureCache.end() && bgfx::isValid(it->second)) {
                dev->blitTexture((uint16_t)viewId, (uint32_t)it->second.idx, cx, cy, cw, ch, opacity);
            }
        }
    }

    if (hasCommandsWrapper) lua_pop(L, 1);
    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.submit_blend(viewId, baseTexId, blendTexId, mode, baseAlpha, blendAlpha, globalAlpha) --
static int lua_Render_submit_blend(lua_State* L) {
    uint16_t viewId    = (uint16_t)luaL_optinteger(L, 1, VIEW_MAIN);
    uint32_t baseTexId = (uint32_t)luaL_checkinteger(L, 2);
    uint32_t blendTexId= (uint32_t)luaL_checkinteger(L, 3);
    int mode           = (int)luaL_optinteger(L, 4, 0);
    float baseAlpha    = (float)luaL_optnumber(L, 5, 1.0);
    float blendAlpha   = (float)luaL_optnumber(L, 6, 1.0);
    float globalAlpha  = (float)luaL_optnumber(L, 7, 1.0);

    auto itBase = g_textureCache.find(baseTexId);
    auto itBlend= g_textureCache.find(blendTexId);
    if (itBase == g_textureCache.end() || itBlend == g_textureCache.end()
        || !bgfx::isValid(itBase->second) || !bgfx::isValid(itBlend->second)) {
        lua_pushboolean(L, 0); return 1;
    }

    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }
    auto* bgfxDev = dynamic_cast<BgfxRenderDevice*>(dev);
    if (!bgfxDev) { lua_pushboolean(L, 0); return 1; }

    bgfxDev->submitBlend(viewId, itBase->second, itBlend->second,
                         mode, baseAlpha, blendAlpha, globalAlpha);
    lua_pushboolean(L, 1); return 1;
}

// -- Render.submit_transition(viewId, fromTexId, toTexId, ruleTexId, method, progress) --
static int lua_Render_submit_transition(lua_State* L) {
    uint16_t viewId   = (uint16_t)luaL_optinteger(L, 1, VIEW_MAIN);
    uint32_t fromTexId= (uint32_t)luaL_checkinteger(L, 2);
    uint32_t toTexId  = (uint32_t)luaL_checkinteger(L, 3);
    uint32_t ruleTexId= (uint32_t)luaL_optinteger(L, 4, 0);
    int method        = (int)luaL_optinteger(L, 5, 0);
    float progress    = (float)luaL_optnumber(L, 6, 0.0);

    auto itFrom = g_textureCache.find(fromTexId);
    auto itTo   = g_textureCache.find(toTexId);
    if (itFrom == g_textureCache.end() || itTo == g_textureCache.end()
        || !bgfx::isValid(itFrom->second) || !bgfx::isValid(itTo->second)) {
        lua_pushboolean(L, 0); return 1;
    }

    bgfx::TextureHandle ruleTex = BGFX_INVALID_HANDLE;
    if (ruleTexId > 0) {
        auto itRule = g_textureCache.find(ruleTexId);
        if (itRule != g_textureCache.end() && bgfx::isValid(itRule->second))
            ruleTex = itRule->second;
    }

    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }
    auto* bgfxDev = dynamic_cast<BgfxRenderDevice*>(dev);
    if (!bgfxDev) { lua_pushboolean(L, 0); return 1; }

    bgfxDev->submitTransition(viewId, itFrom->second, itTo->second,
                              ruleTex, method, progress);
    lua_pushboolean(L, 1); return 1;
}


// -- Render.fill_viewport(handleId, r, g, b, a) ----------------------------

static int lua_Render_fill_viewport(lua_State* L) {
    uint32_t handleId = (uint32_t)luaL_checkinteger(L, 1);
    uint8_t r = (uint8_t)luaL_checkinteger(L, 2);
    uint8_t g = (uint8_t)luaL_checkinteger(L, 3);
    uint8_t b = (uint8_t)luaL_checkinteger(L, 4);
    uint8_t a = (uint8_t)luaL_optinteger(L, 5, 255);

    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }
    auto* bgfxDev = dynamic_cast<BgfxRenderDevice*>(dev);
    if (!bgfxDev) { lua_pushboolean(L, 0); return 1; }

    bgfxDev->fillViewport(ViewportHandle{handleId}, r, g, b, a);
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_Render_submit_vfx(lua_State* L) {
    uint16_t viewId  = (uint16_t)luaL_optinteger(L, 1, VIEW_MAIN);
    uint32_t srcTexId= (uint32_t)luaL_checkinteger(L, 2);
    int effect       = (int)luaL_optinteger(L, 3, 0);
    float fadeAlpha  = (float)luaL_optnumber(L, 4, 0.0);
    float fadeR      = (float)luaL_optnumber(L, 5, 0.0);
    float fadeG      = (float)luaL_optnumber(L, 6, 0.0);
    float fadeB      = (float)luaL_optnumber(L, 7, 0.0);
    float blurRadius = (float)luaL_optnumber(L, 8, 0.0);
    float quakeX     = (float)luaL_optnumber(L, 9, 0.0);
    float quakeY     = (float)luaL_optnumber(L, 10, 0.0);

    auto itSrc = g_textureCache.find(srcTexId);
    if (itSrc == g_textureCache.end() || !bgfx::isValid(itSrc->second)) {
        lua_pushboolean(L, 0); return 1;
    }

    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }
    auto* bgfxDev = dynamic_cast<BgfxRenderDevice*>(dev);
    if (!bgfxDev) { lua_pushboolean(L, 0); return 1; }

    bgfxDev->submitVFX(viewId, itSrc->second, effect,
                       fadeAlpha, fadeR, fadeG, fadeB,
                       blurRadius, quakeX, quakeY);
    lua_pushboolean(L, 1); return 1;
}


// -- Render.free_texture(texId) -------------------------------------------------------

static int lua_Render_free_texture(lua_State* L) {
    return lua_Render_destroy_texture(L);
}

// -- Render.clear_text() ---------------------------------------------------------------

static int lua_Render_clear_text(lua_State* L) {
    IRenderDevice* dev = getRender(L);
    if (dev) dev->renderText(0, "", 0, 0, 0, 0, 0, 0);
    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.render_ruby(text, ruby, x, y) ----------------------------------------------

static int lua_Render_render_ruby(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    const char* ruby = luaL_checkstring(L, 2);
    float x = (float)luaL_optnumber(L, 3, 0);
    float y = (float)luaL_optnumber(L, 4, 0);
    IRenderDevice* dev = getRender(L);
    if (dev) dev->renderRuby(1, text, ruby, x, y, 255, 255, 255, 255);
    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.set_font(id) ---------------------------------------------------------------

static int lua_Render_set_font(lua_State* L) {
    int fontId = (int)luaL_optinteger(L, 1, 0);
    IRenderDevice* dev = getRender(L);
    if (dev) dev->setFont(fontId);
    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.line_height() --------------------------------------------------------------

static int lua_Render_line_height(lua_State* L) {
    IRenderDevice* dev = getRender(L);
    float lh = dev ? dev->textLineHeight() : 24.0f;
    lua_pushnumber(L, (lua_Number)lh);
    return 1;
}

// -- Render.begin_batch() ---------------------------------------------------------------

static int lua_Render_begin_batch(lua_State* L) {
    IRenderDevice* dev = getRender(L);
    if (dev) dev->beginBatch();
    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.flush_batch() ---------------------------------------------------------------

static int lua_Render_flush_batch(lua_State* L) {
    IRenderDevice* dev = getRender(L);
    if (dev) dev->flushBatch();
    lua_pushboolean(L, 1);
    return 1;
}

// -- Render.stretch_blt(dst_texId, dx, dy, dw, dh, src_texId, sx, sy, sw, sh, filter) --
// -- Render.affine_blt(dst_texId, dx, dy, dw, dh, src_texId, sx, sy, sw, sh, mat) ---

static int lua_Render_stretch_blt(lua_State* L) {
    uint32_t dstTexId = (uint32_t)luaL_checkinteger(L, 1);
    float dx = (float)luaL_checknumber(L, 2);
    float dy = (float)luaL_checknumber(L, 3);
    float dw = (float)luaL_checknumber(L, 4);
    float dh = (float)luaL_checknumber(L, 5);
    uint32_t srcTexId = (uint32_t)luaL_checkinteger(L, 6);
    float sx = (float)luaL_checknumber(L, 7);
    float sy = (float)luaL_checknumber(L, 8);
    float sw = (float)luaL_checknumber(L, 9);
    float sh = (float)luaL_checknumber(L, 10);
    int filterType = (int)luaL_optinteger(L, 11, 1);

    auto itSrc = g_textureCache.find(srcTexId);
    if (itSrc == g_textureCache.end() || !bgfx::isValid(itSrc->second)) {
        lua_pushboolean(L, 0); return 1;
    }

    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }

    // Pass raw bgfx handle idx (same convention as blitTexture / submit_batch)
    dev->stretchBlt(VIEW_MAIN, dstTexId,
                     dx, dy, dw, dh,
                     (uint32_t)itSrc->second.idx,
                     sx, sy, sw, sh,
                     filterType);
    lua_pushboolean(L, 1); return 1;
}

static int lua_Render_affine_blt(lua_State* L) {
    uint32_t dstTexId = (uint32_t)luaL_checkinteger(L, 1);
    float dx = (float)luaL_checknumber(L, 2);
    float dy = (float)luaL_checknumber(L, 3);
    float dw = (float)luaL_checknumber(L, 4);
    float dh = (float)luaL_checknumber(L, 5);
    uint32_t srcTexId = (uint32_t)luaL_checkinteger(L, 6);
    float sx = (float)luaL_checknumber(L, 7);
    float sy = (float)luaL_checknumber(L, 8);
    float sw = (float)luaL_checknumber(L, 9);
    float sh = (float)luaL_checknumber(L, 10);

    // Read 2x3 affine matrix from Lua table
    float matrix[6] = { 1, 0, 0, 1, 0, 0 };
    if (lua_istable(L, 11)) {
        lua_getfield(L, 11, "a"); if (lua_isnumber(L, -1)) matrix[0] = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 11, "b"); if (lua_isnumber(L, -1)) matrix[1] = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 11, "c"); if (lua_isnumber(L, -1)) matrix[2] = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 11, "d"); if (lua_isnumber(L, -1)) matrix[3] = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 11, "tx"); if (lua_isnumber(L, -1)) matrix[4] = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 11, "ty"); if (lua_isnumber(L, -1)) matrix[5] = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    } else if (lua_istable(L, 11)) {
        for (int i = 0; i < 6; i++) {
            lua_rawgeti(L, 11, i + 1);
            if (lua_isnumber(L, -1)) matrix[i] = (float)lua_tonumber(L, -1);
            lua_pop(L, 1);
        }
    }

    auto itSrc = g_textureCache.find(srcTexId);
    if (itSrc == g_textureCache.end() || !bgfx::isValid(itSrc->second)) {
        lua_pushboolean(L, 0); return 1;
    }

    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }

    dev->affineBlt(VIEW_MAIN, dstTexId,
                    dx, dy, dw, dh,
                    (uint32_t)itSrc->second.idx,
                    sx, sy, sw, sh,
                    matrix);
    lua_pushboolean(L, 1); return 1;
}

// -- Module registration ----------------------------------------------------

// Forward declaration (defined after module registration)
static int lua_Render_resize(lua_State* L);

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
    { nullptr, nullptr }
};

void registerRenderBinding(lua_State* L) {
    luaL_newlib(L, render_functions);
    lua_setglobal(L, "Render");
    printf("[Lua] Render module registered (via BackendRegistry).\n");
}


// -- Render.resize(w, h) --------------------------------------------------

static int lua_Render_resize(lua_State* L) {
    int w = (int)luaL_checkinteger(L, 1);
    int h = (int)luaL_checkinteger(L, 2);
    if (w <= 0 || h <= 0) { lua_pushboolean(L, 0); return 1; }
    IRenderDevice* dev = getRender(L);
    if (!dev) { lua_pushboolean(L, 0); return 1; }
    dev->resize(w, h);
    printf("[Render] Resized to %dx%d\n", w, h);
    lua_pushboolean(L, 1);
    return 1;
}} // namespace Caesura
