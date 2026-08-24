#pragma once
#include <bgfx/bgfx.h>
#include <string>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <ft2build.h>
#include "api/IRenderDevice.h"
#include "../di/api/IDeviceLostListener.h"
#include FT_FREETYPE_H

namespace Caesura {

// CJK static atlas entry
struct CjkGlyph {
    uint16_t x = 0, y = 0, w = 0, h = 0;
    int16_t advance = 0, offsetX = 0, offsetY = 0;
};

// Resident vertex/index buffer for text layer batching
struct MessageLayerCache {
    bgfx::DynamicVertexBufferHandle vb = BGFX_INVALID_HANDLE;
    bgfx::DynamicIndexBufferHandle  ib = BGFX_INVALID_HANDLE;
    uint32_t maxGlyphs = 2048;
    uint32_t dirtyStart = 0, dirtyEnd = 0, glyphCount = 0;
    std::string cachedText;
    // Full cache key: geometry is only reused when EVERY parameter matches
    // (a single-slot cache keyed on text alone would mis-render the second
    // draw when multiple draws share a text but differ in view/position).
    uint16_t cachedViewId = 0xFFFF;
    float    cachedX = 0.0f, cachedY = 0.0f;
    float    cachedPenAdvance = 0.0f;  // sum of glyph advances for cachedText
    bool matches(uint16_t viewId, const std::string& text, float x, float y) const {
        return cachedViewId == viewId && cachedText == text && cachedX == x && cachedY == y;
    }
    bool        cacheIsCjk = false;     // TD-13: whether cached text uses CJK atlas

    bool isDirty() const { return dirtyStart < dirtyEnd; }
    void markAllDirty() { dirtyStart = 0; dirtyEnd = maxGlyphs; }
    void clearDirty()   { dirtyStart = dirtyEnd = 0; }
};

enum class FontId : uint8_t { Small = 0, Large = 1, TTF = 2 };

struct TextColor {
    uint8_t r, g, b, a;
    static TextColor White()  { return {255,255,255,255}; }
    static TextColor Black()  { return {  0,  0,  0,255}; }
    static TextColor Gray()   { return {128,128,128,255}; }
};

struct TextCursor {
    float x = 32.0f; float y = 48.0f;
    float leftMargin = 32.0f; float lineHeight = 16.0f;
};

struct GlyphMetrics { int x, y, w, h, advance, offsetX, offsetY; };

class TextRenderer : public IDeviceLostListener {
public:
    TextRenderer() = default;
    ~TextRenderer();
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    bool init(IRenderDevice* device);
    void shutdown();

    // TTF loading: loads .ttf, rasterizes ASCII+CJK to runtime atlas, sets FontId::TTF
    bool loadTTF(const char* path, float fontSize = 24.0f);

    void setScreenSize(int w, int h) { m_screenWidth = w; m_screenHeight = h; }
    void setFont(FontId id);
    FontId currentFont() const { return m_currentFont; }
    float lineHeight() const { return m_cursor.lineHeight; }

    void renderText(uint16_t viewId, const std::string& text,
                    float x, float y, TextColor color,
                    float scale = 1.0f, bool bold = false,
                    bool italic = false, bool strike = false);
    void renderRuby(uint16_t viewId, const std::string& text,
                     const std::string& ruby, float x, float y, TextColor color);

    // -- Pure quad geometry (headless-testable) ---------------------------
    // NDC vertex math for a glyph quad: the top edge is sheared right by
    // `shear` px (italic), the bottom edge stays fixed, so advance metrics
    // are unchanged. Extracted as a pure static so unit tests can pin the
    // geometry without a GPU.
    struct NDCQuad { float x0, y0, x1, y1, x2, y2, x3, y3; };
    static NDCQuad glyphQuadToNDC(float x, float y, float w, float h,
                                  float shear, float screenW, float screenH);

    // -- Pure glyph layout (headless-testable) ----------------------------
    // The batch-cache layout math (UTF-8 decode, glyph lookup, quad + UV
    // building, pen advance, NDC conversion) is extracted as pure statics so
    // unit tests can pin it without a GPU. The glyph source is injected via
    // a callback; the production path wraps getTTFGlyph(), tests provide a
    // table. `hasCjk` selects the CJK UV space, `useTtf`/`ttfAscent` drive
    // the baseline offset (mirrors rebuildCache()).
    struct LaidGlyph {
        float gx = 0, gy = 0, w = 0, h = 0;
        float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
        bool fromCjk = false;
    };
    struct GlyphLookupResult {
        GlyphMetrics gm;   // w/h <= 0 means "no glyph" (empty slot)
        bool fromCjk;      // glyph sourced from the CJK atlas space
    };
    using GlyphLookupFn = GlyphLookupResult (*)(uint32_t codepoint, void* userData);
    struct GlyphLayoutResult {
        std::vector<LaidGlyph> glyphs;
        float penAdvance = 0.0f;   // final pen X after all advances
        bool allCjk = false;       // every emitted glyph came from CJK
    };
    // Pure: decode UTF-8 `text`, look up each codepoint, emit quads + UVs.
    // No bgfx/device state is touched. maxGlyphs truncates the output list.
    static GlyphLayoutResult layoutGlyphs(const std::string& text,
                                          float penX, float penY,
                                          GlyphLookupFn lookup, void* userData,
                                          bool hasCjk, float invW, float invH,
                                          float cjkInvW, float cjkInvH,
                                          bool useTtf, float ttfAscent,
                                          size_t maxGlyphs);
    // Pure: turn laid-out glyphs into a triangle-list vertex stream
    // (6 verts x {x,y,u,v} per glyph) + indices. screenW/H <= 0 falls back
    // to raw pixel coordinates (matches rebuildCache()).
    static void buildQuadVertices(const std::vector<LaidGlyph>& glyphs,
                                  float screenW, float screenH,
                                  std::vector<float>& verts,
                                  std::vector<uint32_t>& indices);

    // -- Pure dirty-range math (headless-testable) -------------------------
    // Count UTF-8 codepoints in a byte range (lenient: a truncated trailing
    // sequence counts as one glyph). Used by the batch-cache dirty tracking.
    static uint32_t countUtf8Glyphs(const uint8_t* data, size_t len);

    struct DirtyRangeResult {
        bool changed = false;   // false == texts identical, no rebuild needed
        uint32_t start = 0;     // first changed glyph (codepoint-aligned)
        uint32_t end = 0;       // one past the last changed glyph, <= maxGlyphs
    };
    // Pure: codepoint-aligned dirty range when the cached text changes from
    // oldText to newText. Mirrors updateDirtyRange() exactly, minus the
    // MessageLayerCache member writes.
    static DirtyRangeResult computeDirtyRange(const std::string& oldText,
                                              const std::string& newText,
                                              uint32_t maxGlyphs);

    void newline();
    void clearText(uint16_t viewId);
    void setCursor(float x, float y) { m_cursor.x = x; m_cursor.y = y; }
    void resetCursor() { m_cursor.x = m_cursor.leftMargin; }

    const TextCursor& cursor() const { return m_cursor; }
    bgfx::TextureHandle fontTexture() const { return m_fontTexture; }
    bool isInitialized() const { return m_initialized; }

    // -- Batch-cached rendering (Track 2) --
    float renderTextCached(uint16_t viewId, const std::string& text,
                           float x, float y, TextColor color,
                           bgfx::ProgramHandle program = BGFX_INVALID_HANDLE);
    void invalidateCache();
    const MessageLayerCache& cache() const { return m_msgCache; }

    // -- CJK static atlas (Track 2) --
    bool loadCjkAtlas(const std::string& atlasPath, const std::string& metaPath);
    bool isExpanding() const { return m_expanding; }

    // -- IDeviceLostListener --
    // Releases GPU resources only -- does NOT unregister: the listener list
    // is iterated during notifyDeviceLost (erase during iteration is UB) and
    // restore must still reach us to reinitialize. Defined in the .cpp.
    void onDeviceLost() override;
    void onDeviceRestored() override;

private:
    // GlyphQuad: axis-aligned quad + per-glyph italic shear (top-edge
    // horizontal offset in px; 0 = upright). The bottom edge stays fixed,
    // so advance metrics are unchanged by italics.
    struct GlyphQuad { float x, y, w, h, shear = 0.0f, u0, v0, u1, v1; float advance = 0.0f; };
    GlyphQuad buildGlyph(char ch, float penX, float penY, float scaleW, float scaleH);
    GlyphQuad buildGlyph(uint32_t cp, float penX, float penY, float scaleW, float scaleH);

    void submitGlyphQuads(uint16_t viewId, const GlyphQuad* quads,
                          int count, TextColor color, float scaleW, float scaleH);
    // Strikethrough bars: solid-color quads across the glyph (1x1 white
    // texture; lazily created, destroyed on shutdown/device loss).
    void ensureStrikeTexture();
    void submitStrikeBars(uint16_t viewId, const GlyphQuad* bars,
                          int count, TextColor color);
    bool loadFontAtlas(FontId id);

    // TTF atlas
    struct TTFState {
        ~TTFState();

        FT_Library ftLib = nullptr;
        FT_Face    ftFace = nullptr;
        float ascent = 0.0f, descent = 0.0f, lineGap = 0.0f;
        int atlasW = 2048, atlasH = 2048;
        int penX = 1, penY = 1, maxRowH = 0;
        std::unordered_map<uint32_t, GlyphMetrics> glyphs;
    };
    std::unique_ptr<TTFState> m_ttf;
    bool rasterizeTTFGlyph(uint32_t cp, std::vector<uint8_t>& atlas);

    // -- Glyph lookup (Track 2) --
    GlyphMetrics getTTFGlyph(uint32_t codepoint);
    // GPU-free glyph source used by rebuildCache: resolves via getTTFGlyph()
    // and computes the CJK-sourcing flag. userData = the TextRenderer*.
    static GlyphLookupResult glyphLookupForCache(uint32_t codepoint, void* userData);

    // -- Batch cache internals (Track 2) --
    struct GlyphDraw { float gx, gy, w, h, u0, v0, u1, v1; };
    void updateDirtyRange(const std::string& newText);
    float rebuildCache(uint16_t viewId, const std::string& text,
                       float x, float y, TextColor color,
                       bgfx::ProgramHandle program);
    bool ensureCacheBuffers();

    // -- Track 2 state --
    uint16_t m_atlasW = 2048, m_atlasH = 2048;
    bgfx::TextureHandle m_cjkAtlas = BGFX_INVALID_HANDLE;
    std::unordered_map<uint32_t, CjkGlyph> m_cjkGlyphs;
    bool m_expanding = false;
    bgfx::UniformHandle m_u_color = BGFX_INVALID_HANDLE;
    MessageLayerCache m_msgCache;

    IRenderDevice* m_savedDevice = nullptr;
    bool m_initialized = false;
    FontId m_currentFont = FontId::Small;

    bgfx::TextureHandle m_fontTexture    = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_strikeTexture  = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout  m_posTexLayout;
    bgfx::UniformHandle m_texSampler      = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_fallbackProgram = BGFX_INVALID_HANDLE;

    int m_screenWidth  = 1280;
    int m_screenHeight = 720;
    int m_fontGlyphW = 8, m_fontGlyphH = 16;
    int m_atlasCols = 32;
    float m_ttfFontSize = 24.0f;
    std::string m_ttfPath;

    TextCursor m_cursor;
};

} // namespace Caesura