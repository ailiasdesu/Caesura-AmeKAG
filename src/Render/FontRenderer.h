#pragma once

#include <bgfx/bgfx.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace Caesura {

// ---------------------------------------------------------------------------
// Glyph -- atlas sub-rectangle + metrics for a single codepoint
// ---------------------------------------------------------------------------

struct FontGlyph {
    uint16_t x = 0;       // Pixel offset within atlas
    uint16_t y = 0;
    uint16_t w = 0;       // Glyph bitmap width
    uint16_t h = 0;       // Glyph bitmap height
    int16_t  advance = 0; // Horizontal advance (pixels)
    int16_t  offsetX = 0; // Left-side bearing (pixels)
    int16_t  offsetY = 0; // Top-side bearing from baseline (pixels)
};

// ---------------------------------------------------------------------------
// CjkGlyph -- pre-built CJK static atlas entry (G8-U5)
// Loaded from a pre-generated atlas binary at init.  Used as a fallback
// layer during dynamic atlas expansion so CJK text never degrades to
// built-in bitmap unless the character truly isn''t available.
// ---------------------------------------------------------------------------

struct CjkGlyph {
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    int16_t  advance = 0;
    int16_t  offsetX = 0;
    int16_t  offsetY = 0;
};

// ---------------------------------------------------------------------------
// PosTexVertex -- 2D position + UV for glyph quad submission
// ---------------------------------------------------------------------------


struct PosTexVertex {
    float x, y;
    float u, v;
};

// ---------------------------------------------------------------------------
// MessageLayerCache -- resident vertex/index buffer for text layer batching
// ---------------------------------------------------------------------------

struct MessageLayerCache {
    bgfx::DynamicVertexBufferHandle vb = BGFX_INVALID_HANDLE;
    bgfx::DynamicIndexBufferHandle  ib = BGFX_INVALID_HANDLE;
    uint32_t maxGlyphs = 2048;
    uint32_t dirtyStart = 0;
    uint32_t dirtyEnd   = 0;
    uint32_t glyphCount = 0;
    std::string cachedText;

    bool isDirty() const { return dirtyStart < dirtyEnd; }
    void markAllDirty() { dirtyStart = 0; dirtyEnd = maxGlyphs; }
    void clearDirty()   { dirtyStart = dirtyEnd = 0; }
};

// ---------------------------------------------------------------------------
// FontRenderer -- TrueType font atlas and text rendering via FreeType 2
// ---------------------------------------------------------------------------
// Rasterizes glyphs into a 2048x2048 RGBA8 texture atlas on demand.
// Pre-fills ASCII 32-126 at init.  CJK / additional ranges are added lazily
// on first render.  Submits textured quads directly to the GPU through bgfx.
//
// Architecture invariant: all bgfx calls happen on main thread only.

class FontRenderer {
public:
    static FontRenderer& instance();

    FontRenderer(const FontRenderer&) = delete;
    FontRenderer& operator=(const FontRenderer&) = delete;

    // -- Lifecycle ---------------------------------------------------------
    bool init(const std::string& fontPath, float fontSize = 24.0f);
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // -- Glyph lookup ------------------------------------------------------
    FontGlyph getGlyph(char32_t codepoint);

    // -- Texture atlas access ----------------------------------------------
    bgfx::TextureHandle atlas() const { return m_atlas; }
    uint16_t atlasWidth()  const { return m_atlasW; }
    uint16_t atlasHeight() const { return m_atlasH; }

    // -- Text rendering ----------------------------------------------------
    float renderText(uint16_t viewId, const std::string& text,
                     float x, float y,
                     uint8_t cr = 255, uint8_t cg = 255,
                     uint8_t cb = 255, uint8_t ca = 255,
                     bgfx::ProgramHandle program = BGFX_INVALID_HANDLE);

    // -- Batch-cached text rendering ---------------------------------------
    float renderTextCached(uint16_t viewId, const std::string& text,
                           float x, float y,
                           uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca,
                           bgfx::ProgramHandle program);

    // -- Batch cache management --------------------------------------------
    void invalidateCache();
    const MessageLayerCache& cache() const { return m_msgCache; }

    // -- CJK static atlas (G8-U5) ------------------------------------------
    bool loadCjkAtlas(const std::string& atlasPath, const std::string& metaPath);
    bool isExpanding() const { return m_expanding; }

    // -- Metrics -----------------------------------------------------------
    float lineHeight()     const { return m_lineHeight; }
    float ascender()       const { return m_ascender; }
    float fontSize()       const { return m_fontSize; }

private:
    FontRenderer() = default;

    bool rasterizeGlyph(char32_t cp);
    bool ensureGlyph(char32_t cp);

    void updateDirtyRange(const std::string& newText);
    float rebuildCache(uint16_t viewId, const std::string& text,
                       float x, float y,
                       uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca,
                       bgfx::ProgramHandle program);
    bool ensureCacheBuffers();

    // -- Atlas -----------------------------------------------------------------
    bgfx::TextureHandle m_atlas       = BGFX_INVALID_HANDLE;
    static constexpr uint16_t m_atlasW = 2048;
    static constexpr uint16_t m_atlasH = 2048;
    uint16_t m_penX = 1, m_penY = 1, m_maxRowH = 0;

    // -- Font state (FreeType 2) -----------------------------------------------
    FT_Library m_ftLibrary = nullptr;
    FT_Face    m_ftFace    = nullptr;
    float m_fontSize   = 24.0f;
    float m_ascender   = 0.0f;
    float m_descender  = 0.0f;
    float m_lineHeight = 0.0f;

    // -- Glyph cache -----------------------------------------------------------
    std::unordered_map<uint32_t, FontGlyph> m_cache;

    // -- CJK static atlas fallback (G8-U5) --------------------------------------
    bgfx::TextureHandle m_cjkAtlas = BGFX_INVALID_HANDLE;
    std::unordered_map<uint32_t, CjkGlyph> m_cjkGlyphs;
    bool m_expanding = false;  // true while dynamic atlas is resizing

    // -- bgfx resources for quad submission ------------------------------------
    bgfx::VertexLayout   m_layout;
    bgfx::ProgramHandle  m_program  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle  m_u_color  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle  m_u_tex    = BGFX_INVALID_HANDLE;

    // -- Message layer batch cache ----------------------------------------------
    MessageLayerCache m_msgCache;

    bool m_initialized = false;
};

} // namespace Caesura