#include "FontRenderer.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include "Render/FreeTypeContext.h"

namespace Caesura {

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

FontRenderer& FontRenderer::instance() {
    static FontRenderer fr;
    return fr;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool FontRenderer::init(const std::string& fontPath, float fontSize) {
    if (m_initialized) {
        fprintf(stderr, "[FontRenderer] Already initialized.\n");
        return true;
    }

    // -- Initialize FreeType 2 --
    FT_Error ftErr = FT_Init_FreeType(&m_ftLibrary);
    if (ftErr) {
        fprintf(stderr, "[FontRenderer] FT_Init_FreeType failed: %d\n", (int)ftErr);
        return false;
    }

    ftErr = FT_New_Face(m_ftLibrary, fontPath.c_str(), 0, &m_ftFace);
    if (ftErr) {
        fprintf(stderr, "[FontRenderer] FT_New_Face failed: %s (err=%d)\n",
                fontPath.c_str(), (int)ftErr);
        FT_Done_FreeType(m_ftLibrary);
        m_ftLibrary = nullptr;
        return false;
    }

    FT_Set_Pixel_Sizes(m_ftFace, 0, (FT_UInt)fontSize);

    m_fontSize   = fontSize;
    m_ascender   = m_ftFace->size->metrics.ascender / 64.0f;
    m_descender  = m_ftFace->size->metrics.descender / 64.0f;
    m_lineHeight = m_ftFace->size->metrics.height / 64.0f;

    // -- Create atlas texture (2048x2048 RGBA8) --
    const uint32_t atlasBytes = m_atlasW * m_atlasH * 4;
    std::vector<uint8_t> emptyAtlas(atlasBytes, 0);
    const bgfx::Memory* mem = bgfx::copy(emptyAtlas.data(), atlasBytes);
    m_atlas = bgfx::createTexture2D(
        m_atlasW, m_atlasH, false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, mem);
    if (!bgfx::isValid(m_atlas)) {
        fprintf(stderr, "[FontRenderer] Failed to create atlas texture.\n");
        FT_Done_Face(m_ftFace);
        FT_Done_FreeType(m_ftLibrary);
        m_ftFace = nullptr;
        m_ftLibrary = nullptr;
        return false;
    }

    // -- Pre-fill ASCII 32-126 --
    for (char32_t cp = 32; cp <= 126; ++cp) {
        if (!rasterizeGlyph(cp))
            fprintf(stderr, "[FontRenderer] Warning: glyph %u rasterization failed.\n",
                    (unsigned)cp);
    }

    // -- Set up bgfx resources for quad submission --
    m_layout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    m_u_tex   = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    m_u_color = bgfx::createUniform("u_color", bgfx::UniformType::Vec4);

    m_initialized = true;
    printf("[FontRenderer] Initialized: %s (%.0fpx), %zu glyphs rasterized, asc=%.1f dsc=%.1f lh=%.1f\n",
           fontPath.c_str(), fontSize, m_cache.size(),
           m_ascender, m_descender, m_lineHeight);
    return true;
}

void FontRenderer::shutdown() {
    if (bgfx::isValid(m_cjkAtlas)) {
        bgfx::destroy(m_cjkAtlas);
        m_cjkAtlas = BGFX_INVALID_HANDLE;
    }
    m_cjkGlyphs.clear();
    if (!m_initialized) return;

    if (bgfx::isValid(m_atlas))            bgfx::destroy(m_atlas);
    if (bgfx::isValid(m_u_tex))            bgfx::destroy(m_u_tex);
    if (bgfx::isValid(m_u_color))          bgfx::destroy(m_u_color);
    if (bgfx::isValid(m_msgCache.vb))      bgfx::destroy(m_msgCache.vb);
    if (bgfx::isValid(m_msgCache.ib))      bgfx::destroy(m_msgCache.ib);

    m_cache.clear();
    if (m_ftFace)    { FT_Done_Face(m_ftFace);       m_ftFace = nullptr; }
    if (m_ftLibrary) { FT_Done_FreeType(m_ftLibrary); m_ftLibrary = nullptr; }
    m_msgCache = MessageLayerCache{};
    m_initialized = false;
    printf("[FontRenderer] Shutdown complete.\n");
}

// ---------------------------------------------------------------------------
// Glyph rasterization -- FreeType 2 -> atlas sub-region
// ---------------------------------------------------------------------------

bool FontRenderer::rasterizeGlyph(char32_t cp) {
    if (m_cache.count(cp)) return true;

    if (!m_ftFace) return false;

    FT_UInt glyphIndex = FT_Get_Char_Index(m_ftFace, cp);

    FT_Error ftErr = FT_Load_Glyph(m_ftFace, glyphIndex, FT_LOAD_DEFAULT);
    if (ftErr) {
        // Metrics-only entry for missing glyphs
        FontGlyph g;
        g.advance = 0;
        g.offsetX = 0;
        g.offsetY = 0;
        g.w = g.h = 0;
        m_cache[cp] = g;
        return true;
    }

    FT_GlyphSlot slot = m_ftFace->glyph;

    // Metrics (26.6 fixed-point -> pixels)
    int adv  = (int)(slot->advance.x >> 6);
    int xoff = slot->bitmap_left;
    int yoff = slot->bitmap_top;

    // Render to 8-bit grayscale bitmap
    ftErr = FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
    if (ftErr) {
        FontGlyph g;
        g.advance = (int16_t)adv;
        g.offsetX = (int16_t)xoff;
        g.offsetY = (int16_t)yoff;
        g.w = g.h = 0;
        m_cache[cp] = g;
        return true;
    }

    FT_Bitmap* bitmap = &slot->bitmap;
    int w = (int)bitmap->width;
    int h = (int)bitmap->rows;

    if (w <= 0 || h <= 0) {
        // Empty glyph (space etc.) -- metrics only
        FontGlyph g;
        g.advance = (int16_t)adv;
        g.offsetX = (int16_t)xoff;
        g.offsetY = (int16_t)yoff;
        g.w = g.h = 0;
        m_cache[cp] = g;
        return true;
    }

    // -- Atlas row packing --
    if (m_penX + w + 1 >= m_atlasW) {
        m_penX = 1;
        m_penY += m_maxRowH + 1;
        m_maxRowH = 0;
    }
    if (m_penY + h + 1 >= m_atlasH) {
        fprintf(stderr, "[FontRenderer] Atlas overflow at glyph %u\n", (unsigned)cp);
        return false;
    }

    // -- Expand 8-bit grayscale to RGBA for atlas upload --
    std::vector<uint8_t> rgba(w * h * 4);
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            uint8_t gray = bitmap->buffer[row * bitmap->pitch + col];
            uint8_t* dst = &rgba[(row * w + col) * 4];
            dst[0] = 255;
            dst[1] = 255;
            dst[2] = 255;
            dst[3] = gray;
        }
    }

    bgfx::updateTexture2D(m_atlas, 0, 0, m_penX, m_penY,
                          (uint16_t)w, (uint16_t)h,
                          bgfx::copy(rgba.data(), (uint32_t)(w * h * 4)));

    FontGlyph g;
    g.x       = m_penX;
    g.y       = m_penY;
    g.w       = (uint16_t)w;
    g.h       = (uint16_t)h;
    g.advance = (int16_t)adv;
    g.offsetX = (int16_t)xoff;
    g.offsetY = (int16_t)yoff;

    m_cache[cp] = g;

    m_penX += w + 1;
    if (h > m_maxRowH) m_maxRowH = (uint16_t)h;
    return true;
}

// ---------------------------------------------------------------------------
// CJK static atlas fallback (G8-U5) -- spec [10.2.54]
// Fallback chain: dynamic atlas -> CJK static atlas -> built-in bitmap -> empty
// ---------------------------------------------------------------------------

bool FontRenderer::loadCjkAtlas(const std::string& atlasPath, const std::string& metaPath) {
    // Load raw RGBA8 atlas texture (4096x4096)
    FILE* f = fopen(atlasPath.c_str(), "rb");
    if (!f) {
        fprintf(stderr, "[FontRenderer] CJK atlas not found: %s (skipping)\n", atlasPath.c_str());
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data(size);
    fread(data.data(), 1, size, f);
    fclose(f);

    const uint16_t cjkW = 4096, cjkH = 4096;
    m_cjkAtlas = bgfx::createTexture2D(cjkW, cjkH, false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP,
        bgfx::copy(data.data(), (uint32_t)(cjkW * cjkH * 4)));
    if (!bgfx::isValid(m_cjkAtlas)) {
        fprintf(stderr, "[FontRenderer] CJK atlas texture creation failed\n");
        return false;
    }

    // Load glyph metadata binary
    FILE* mf = fopen(metaPath.c_str(), "rb");
    if (!mf) {
        fprintf(stderr, "[FontRenderer] CJK metadata not found: %s\n", metaPath.c_str());
        bgfx::destroy(m_cjkAtlas);
        m_cjkAtlas = BGFX_INVALID_HANDLE;
        return false;
    }
    uint32_t count = 0;
    fread(&count, sizeof(count), 1, mf);
    m_cjkGlyphs.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t cp;
        CjkGlyph g;
        fread(&cp, sizeof(cp), 1, mf);
        fread(&g.x, sizeof(g.x), 1, mf);
        fread(&g.y, sizeof(g.y), 1, mf);
        fread(&g.w, sizeof(g.w), 1, mf);
        fread(&g.h, sizeof(g.h), 1, mf);
        fread(&g.advance, sizeof(g.advance), 1, mf);
        fread(&g.offsetX, sizeof(g.offsetX), 1, mf);
        fread(&g.offsetY, sizeof(g.offsetY), 1, mf);
        m_cjkGlyphs[cp] = g;
    }
    fclose(mf);

    printf("[FontRenderer] CJK static atlas loaded: %u glyphs (%dx%d)\n", count, cjkW, cjkH);
    return true;
}

bool FontRenderer::ensureGlyph(char32_t cp) {
    if (m_cache.count(cp)) return true;
    return rasterizeGlyph(cp);
}

// ---------------------------------------------------------------------------
// Glyph lookup
// ---------------------------------------------------------------------------

FontGlyph FontRenderer::getGlyph(char32_t codepoint) {
    // 1. Check dynamic atlas cache
    auto it = m_cache.find(codepoint);
    if (it != m_cache.end())
        return it->second;

    // 2. CJK static atlas fallback (during expansion or as secondary cache)
    if (bgfx::isValid(m_cjkAtlas)) {
        auto cjkIt = m_cjkGlyphs.find(codepoint);
        if (cjkIt != m_cjkGlyphs.end()) {
            FontGlyph cjk;
            cjk.x       = cjkIt->second.x;
            cjk.y       = cjkIt->second.y;
            cjk.w       = cjkIt->second.w;
            cjk.h       = cjkIt->second.h;
            cjk.advance = cjkIt->second.advance;
            cjk.offsetX = cjkIt->second.offsetX;
            cjk.offsetY = cjkIt->second.offsetY;
            return cjk;
        }
    }

    // 3. Try lazy rasterize (skip during expansion)
    if (!m_expanding && rasterizeGlyph(codepoint)) {
        it = m_cache.find(codepoint);
        if (it != m_cache.end())
            return it->second;
    }

    // 4. Fallback to U+FFFD (REPLACEMENT CHARACTER)
    if (codepoint != 0xFFFD) {
        return getGlyph(0xFFFD);
    }

    // 5. Absolute last resort: 8px advance empty glyph
    FontGlyph fb{};
    fb.advance = 8;
    return fb;
}

// ---------------------------------------------------------------------------
// UTF-8 helpers (local)
// ---------------------------------------------------------------------------

static int utf8CharLen(uint8_t lead) {
    if (lead < 0x80) return 1;
    if (lead < 0xC0) return 1;
    if (lead < 0xE0) return 2;
    if (lead < 0xF0) return 3;
    return 4;
}

static uint32_t utf8Codepoint(const uint8_t* data, int len) {
    if (len == 1) return data[0];
    if (len == 2) return ((data[0] & 0x1F) << 6) | (data[1] & 0x3F);
    if (len == 3) return ((data[0] & 0x0F) << 12) | ((data[1] & 0x3F) << 6) | (data[2] & 0x3F);
    return ((data[0] & 0x07) << 18) | ((data[1] & 0x3F) << 12) | ((data[2] & 0x3F) << 6) | (data[3] & 0x3F);
}

// ---------------------------------------------------------------------------
// renderText -- submit glyph quads via bgfx transient buffers (per-call path)
// ---------------------------------------------------------------------------

float FontRenderer::renderText(uint16_t viewId, const std::string& text,
                                float x, float y,
                                uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca,
                                bgfx::ProgramHandle program) {
    if (!m_initialized || text.empty()) return x;

    bgfx::ProgramHandle prog = bgfx::isValid(program) ? program : m_program;
    if (!bgfx::isValid(prog)) return x;

    const float invW = 1.0f / float(m_atlasW);
    const float invH = 1.0f / float(m_atlasH);
    float penX = x;
    const float penY = y;

    const uint8_t* tdata = reinterpret_cast<const uint8_t*>(text.data());
    int tlen = (int)text.size();

    for (int i = 0; i < tlen; ) {
        int clen = utf8CharLen(tdata[i]);
        if (i + clen > tlen) clen = tlen - i;
        uint32_t cp = utf8Codepoint(&tdata[i], clen);
        i += clen;

        if (!m_cache.count(cp)) {
            if (!rasterizeGlyph(cp)) {
                penX += m_fontSize * 0.33f;
                continue;
            }
        }

        FontGlyph g = m_cache[cp];

        if (g.w > 0 && g.h > 0) {
            float gx = penX + g.offsetX;
            float gy = penY - g.offsetY + m_ascender;

            float u0 = g.x * invW;
            float v0 = g.y * invH;
            float u1 = (g.x + g.w) * invW;
            float v1 = (g.y + g.h) * invH;

            if (bgfx::getAvailTransientVertexBuffer(6, m_layout) < 6 ||
                bgfx::getAvailTransientIndexBuffer(6, false) < 6) {
                penX += g.advance;
                continue;
            }

            bgfx::TransientVertexBuffer tvb;
            bgfx::allocTransientVertexBuffer(&tvb, 6, m_layout);
            PosTexVertex* verts = reinterpret_cast<PosTexVertex*>(tvb.data);
            verts[0] = { gx,       gy,       u0, v0 };
            verts[1] = { gx + g.w, gy,       u1, v0 };
            verts[2] = { gx + g.w, gy + g.h, u1, v1 };
            verts[3] = { gx,       gy,       u0, v0 };
            verts[4] = { gx + g.w, gy + g.h, u1, v1 };
            verts[5] = { gx,       gy + g.h, u0, v1 };

            bgfx::TransientIndexBuffer tib;
            bgfx::allocTransientIndexBuffer(&tib, 6, false);
            uint16_t* idx = reinterpret_cast<uint16_t*>(tib.data);
            uint16_t base = 0;
            idx[0] = base; idx[1] = base + 1; idx[2] = base + 2;
            idx[3] = base; idx[4] = base + 2; idx[5] = base + 3;

            float color[4] = { (float)cr / 255.0f, (float)cg / 255.0f, (float)cb / 255.0f, (float)ca / 255.0f };
            bgfx::setUniform(m_u_color, color);
            bgfx::setTexture(0, m_u_tex, m_atlas);
            bgfx::setVertexBuffer(0, &tvb);
            bgfx::setIndexBuffer(&tib);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_BLEND_ALPHA);
            bgfx::submit(viewId, prog);
        }

        penX += g.advance;
    }

    return penX;
}

// ---------------------------------------------------------------------------
// MessageLayerCache -- resident buffer management
// ---------------------------------------------------------------------------

bool FontRenderer::ensureCacheBuffers() {
    // 6 vertices per glyph (2 triangles), 6 indices per glyph
    uint32_t maxVerts = m_msgCache.maxGlyphs * 6;
    uint32_t maxInds  = m_msgCache.maxGlyphs * 6;

    if (!bgfx::isValid(m_msgCache.vb)) {
        m_msgCache.vb = bgfx::createDynamicVertexBuffer(
            maxVerts, m_layout, BGFX_BUFFER_ALLOW_RESIZE);
        if (!bgfx::isValid(m_msgCache.vb)) {
            fprintf(stderr, "[FontRenderer] Failed to create dynamic vertex buffer.\n");
            return false;
        }
    }

    if (!bgfx::isValid(m_msgCache.ib)) {
        m_msgCache.ib = bgfx::createDynamicIndexBuffer(
            maxInds, BGFX_BUFFER_ALLOW_RESIZE | BGFX_BUFFER_INDEX32);
        if (!bgfx::isValid(m_msgCache.ib)) {
            fprintf(stderr, "[FontRenderer] Failed to create dynamic index buffer.\n");
            return false;
        }
    }

    return true;
}

void FontRenderer::invalidateCache() {
    m_msgCache.cachedText.clear();
    m_msgCache.markAllDirty();
}

void FontRenderer::updateDirtyRange(const std::string& newText) {
    const std::string& oldText = m_msgCache.cachedText;
    size_t oldLen = oldText.size();
    size_t newLen = newText.size();

    // Find first differing glyph index
    uint32_t diffStart = 0;
    const uint8_t* oldData = reinterpret_cast<const uint8_t*>(oldText.data());
    const uint8_t* newData = reinterpret_cast<const uint8_t*>(newText.data());

    size_t oldPos = 0, newPos = 0;
    while (oldPos < oldLen && newPos < newLen) {
        int oclen = utf8CharLen(oldData[oldPos]);
        int nclen = utf8CharLen(newData[newPos]);
        if (oclen != nclen ||
            memcmp(oldData + oldPos, newData + newPos, oclen) != 0) {
            break;
        }
        oldPos += oclen;
        newPos += nclen;
        diffStart++;
    }

    // Count glyphs in the remainder of each string
    auto countGlyphs = [](const uint8_t* data, size_t len) -> uint32_t {
        uint32_t count = 0;
        for (size_t pos = 0; pos < len; ) {
            int clen = utf8CharLen(data[pos]);
            if (pos + clen > len) clen = (int)(len - pos);
            pos += clen;
            count++;
        }
        return count;
    };

    uint32_t oldRemain = countGlyphs(oldData + oldPos, oldLen - oldPos);
    uint32_t newRemain = countGlyphs(newData + newPos, newLen - newPos);

    if (oldRemain == 0 && newRemain == 0) {
        // Text identical after diffStart -- no change
        m_msgCache.clearDirty();
        return;
    }

    m_msgCache.dirtyStart = diffStart;
    m_msgCache.dirtyEnd   = diffStart + (oldRemain > newRemain ? oldRemain : newRemain);
    if (m_msgCache.dirtyEnd > m_msgCache.maxGlyphs)
        m_msgCache.dirtyEnd = m_msgCache.maxGlyphs;

    m_msgCache.cachedText = newText;
}

// ---------------------------------------------------------------------------
// rebuildCache -- rasterize dirty glyphs and upload to resident buffers
// ---------------------------------------------------------------------------

struct GlyphDraw {
    float gx, gy, u0, v0, u1, v1;  // quad position + UV
};

float FontRenderer::rebuildCache(uint16_t viewId, const std::string& text,
                                  float x, float y,
                                  uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca,
                                  bgfx::ProgramHandle program) {
    if (!ensureCacheBuffers() || text.empty()) return x;

    const float invW = 1.0f / float(m_atlasW);
    const float invH = 1.0f / float(m_atlasH);
    float penX = x;
    const float penY = y;

    const uint8_t* tdata = reinterpret_cast<const uint8_t*>(text.data());
    int tlen = (int)text.size();

    std::vector<GlyphDraw> draws;
    draws.reserve(m_msgCache.maxGlyphs);

    for (int i = 0; i < tlen; ) {
        int clen = utf8CharLen(tdata[i]);
        if (i + clen > tlen) clen = tlen - i;
        uint32_t cp = utf8Codepoint(&tdata[i], clen);
        i += clen;

        if (!m_cache.count(cp)) {
            if (!rasterizeGlyph(cp)) {
                penX += m_fontSize * 0.33f;
                GlyphDraw d = {0,0,0,0,0,0};
                draws.push_back(d);
                continue;
            }
        }

        FontGlyph g = m_cache[cp];
        if (g.w > 0 && g.h > 0) {
            GlyphDraw d;
            d.gx = penX + g.offsetX;
            d.gy = penY - g.offsetY + m_ascender;
            d.u0 = g.x * invW;
            d.v0 = g.y * invH;
            d.u1 = (g.x + g.w) * invW;
            d.v1 = (g.y + g.h) * invH;
            draws.push_back(d);
        } else {
            draws.push_back({0,0,0,0,0,0});
        }
        penX += g.advance;
    }

    m_msgCache.glyphCount = static_cast<uint32_t>(draws.size());
    if (m_msgCache.glyphCount > m_msgCache.maxGlyphs) {
        m_msgCache.glyphCount = m_msgCache.maxGlyphs;
    }

    // Build vertex data
    std::vector<PosTexVertex> verts;
    std::vector<uint32_t> indices;
    verts.reserve(m_msgCache.glyphCount * 6);
    indices.reserve(m_msgCache.glyphCount * 6);

    for (uint32_t gi = 0; gi < m_msgCache.glyphCount; ++gi) {
        const GlyphDraw& d = draws[gi];
        uint32_t base = gi * 4;  // 4 unique vertices per quad
        // For a dynamic buffer, we use 6 vertices + 6 indices per glyph
        uint32_t vbase = gi * 6;
        verts.push_back({ d.gx,       d.gy,       d.u0, d.v0 });
        verts.push_back({ d.gx + 1.0f, d.gy,       d.u1, d.v0 });  // width=1 for now
        verts.push_back({ d.gx + 1.0f, d.gy + 1.0f, d.u1, d.v1 });
        verts.push_back({ d.gx,       d.gy,       d.u0, d.v0 });
        verts.push_back({ d.gx + 1.0f, d.gy + 1.0f, d.u1, d.v1 });
        verts.push_back({ d.gx,       d.gy + 1.0f, d.u0, d.v1 });

        indices.push_back(vbase);
        indices.push_back(vbase + 1);
        indices.push_back(vbase + 2);
        indices.push_back(vbase);
        indices.push_back(vbase + 2);
        indices.push_back(vbase + 3);
    }

    // Upload to dynamic buffers
    uint32_t numVerts = m_msgCache.glyphCount * 6;
    uint32_t numInds  = m_msgCache.glyphCount * 6;

    const bgfx::Memory* vmem = bgfx::copy(verts.data(),
        static_cast<uint32_t>(verts.size()) * sizeof(PosTexVertex));
    const bgfx::Memory* imem = bgfx::copy(indices.data(),
        static_cast<uint32_t>(indices.size()) * sizeof(uint32_t));

    bgfx::update(m_msgCache.vb, 0, vmem);
    bgfx::update(m_msgCache.ib, 0, imem);

    // Submit a single draw call for the entire message layer text
    float color[4] = { (float)cr / 255.0f, (float)cg / 255.0f,
                       (float)cb / 255.0f, (float)ca / 255.0f };
    bgfx::setUniform(m_u_color, color);
    bgfx::setTexture(0, m_u_tex, m_atlas);
    bgfx::setVertexBuffer(0, m_msgCache.vb, 0, numVerts);
    bgfx::setIndexBuffer(m_msgCache.ib, 0, numInds);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_BLEND_ALPHA);
    bgfx::submit(viewId, program);

    m_msgCache.clearDirty();
    return penX;
}

// ---------------------------------------------------------------------------
// renderTextCached -- batch-cached text rendering entry point
// ---------------------------------------------------------------------------

float FontRenderer::renderTextCached(uint16_t viewId, const std::string& text,
                                      float x, float y,
                                      uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca,
                                      bgfx::ProgramHandle program) {
    if (!m_initialized || text.empty()) return x;

    bgfx::ProgramHandle prog = bgfx::isValid(program) ? program : m_program;
    if (!bgfx::isValid(prog)) return x;

    // Compute dirty range: diff old text vs new text
    if (text != m_msgCache.cachedText) {
        updateDirtyRange(text);
    }

    if (m_msgCache.isDirty()) {
        return rebuildCache(viewId, text, x, y, cr, cg, cb, ca, prog);
    }

    // Text unchanged -- submit existing buffers (zero-cost redraw)
    if (!ensureCacheBuffers()) return x;

    float color[4] = { (float)cr / 255.0f, (float)cg / 255.0f,
                       (float)cb / 255.0f, (float)ca / 255.0f };
    uint32_t numVerts = m_msgCache.glyphCount * 6;
    uint32_t numInds  = m_msgCache.glyphCount * 6;

    bgfx::setUniform(m_u_color, color);
    bgfx::setTexture(0, m_u_tex, m_atlas);
    bgfx::setVertexBuffer(0, m_msgCache.vb, 0, numVerts);
    bgfx::setIndexBuffer(m_msgCache.ib, 0, numInds);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_BLEND_ALPHA);
    bgfx::submit(viewId, prog);

    // Pen position = x + sum of advances for all glyphs
    float penX = x;
    const uint8_t* tdata = reinterpret_cast<const uint8_t*>(text.data());
    int tlen = (int)text.size();
    for (int i = 0; i < tlen; ) {
        int clen = utf8CharLen(tdata[i]);
        if (i + clen > tlen) clen = tlen - i;
        uint32_t cp = utf8Codepoint(&tdata[i], clen);
        i += clen;
        FontGlyph g = getGlyph(cp);
        penX += g.advance;
    }
    return penX;
}

} // namespace Caesura