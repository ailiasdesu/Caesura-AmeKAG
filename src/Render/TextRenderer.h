#pragma once
#include <bgfx/bgfx.h>
#include <string>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace Caesura {

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

class TextRenderer {
public:
    TextRenderer() = default;
    ~TextRenderer();
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    bool init(class BgfxRenderDevice* device);
    void shutdown();

    // TTF loading: loads .ttf, rasterizes ASCII+CJK to runtime atlas, sets FontId::TTF
    bool loadTTF(const char* path, float fontSize = 24.0f);

    void setScreenSize(int w, int h) { m_screenWidth = w; m_screenHeight = h; }
    void setFont(FontId id);
    FontId currentFont() const { return m_currentFont; }
    float lineHeight() const { return m_cursor.lineHeight; }

    void renderText(uint16_t viewId, const std::string& text,
                    float x, float y, TextColor color);
    void renderRuby(uint16_t viewId, const std::string& text,
                     const std::string& ruby, float x, float y, TextColor color);

    void newline();
    void clearText(uint16_t viewId);
    void setCursor(float x, float y) { m_cursor.x = x; m_cursor.y = y; }
    void resetCursor() { m_cursor.x = m_cursor.leftMargin; }

    const TextCursor& cursor() const { return m_cursor; }
    bgfx::TextureHandle fontTexture() const { return m_fontTexture; }
    bool isInitialized() const { return m_initialized; }

private:
    struct GlyphQuad { float x, y, w, h, u0, v0, u1, v1; };
    GlyphQuad buildGlyph(char ch, float penX, float penY, float scaleW, float scaleH);
    GlyphQuad buildGlyph(uint32_t cp, float penX, float penY, float scaleW, float scaleH);

    void submitGlyphQuads(uint16_t viewId, const GlyphQuad* quads,
                          int count, TextColor color, float scaleW, float scaleH);
    bool loadFontAtlas(FontId id);

    // TTF atlas
    struct TTFState {
        FT_Library ftLib = nullptr;
        FT_Face    ftFace = nullptr;
        float ascent = 0.0f, descent = 0.0f, lineGap = 0.0f;
        int atlasW = 1024, atlasH = 1024;
        int penX = 1, penY = 1, maxRowH = 0;
        std::unordered_map<uint32_t, GlyphMetrics> glyphs;
    };
    std::unique_ptr<TTFState> m_ttf;
    bool rasterizeTTFGlyph(uint32_t cp, std::vector<uint8_t>& atlas);

    bool m_initialized = false;
    FontId m_currentFont = FontId::Small;

    bgfx::TextureHandle m_fontTexture    = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout  m_posTexLayout;
    bgfx::UniformHandle m_texSampler      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_texColor      = BGFX_INVALID_HANDLE;  // borrowed from BgfxRenderDevice
    bgfx::ProgramHandle m_fallbackProgram = BGFX_INVALID_HANDLE;

    int m_screenWidth  = 1280;
    int m_screenHeight = 720;
    int m_fontGlyphW = 8, m_fontGlyphH = 16;
    int m_atlasCols = 32;
    float m_ttfFontSize = 24.0f;

    TextCursor m_cursor;
};

} // namespace Caesura
