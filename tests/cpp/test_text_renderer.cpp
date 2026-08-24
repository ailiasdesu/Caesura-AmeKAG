// =============================================================================
// test_text_renderer.cpp — GPU-free unit tests for TextRenderer's pure
// glyph-layout math (P2-10 closure).
//
// layoutGlyphs() / buildQuadVertices() were extracted from rebuildCache() so
// the UTF-8 decode, glyph lookup, quad/UV building, pen advance and NDC
// conversion can be pinned without a GPU. The glyph source is injected via a
// GlyphLookupFn; tests use a fixed table instead of FreeType/bgfx state.
// =============================================================================

#include "doctest.h"

#include "render/TextRenderer.h"

#include <string>
#include <vector>
#include <cstdint>

using namespace Caesura;

namespace {

// Fixed glyph table for the injected lookup: ASCII 'A'..'C' plus CJK '中'.
struct TableGlyph {
    GlyphMetrics gm;
    bool fromCjk;
};

GlyphMetrics makeGm(int x, int y, int w, int h, int advance, int ox = 0, int oy = 0) {
    GlyphMetrics gm;
    gm.x = x; gm.y = y; gm.w = w; gm.h = h;
    gm.advance = advance; gm.offsetX = ox; gm.offsetY = oy;
    return gm;
}

TextRenderer::GlyphLookupResult lookupFromTable(uint32_t cp, void* userData) {
    const auto* table = static_cast<const std::unordered_map<uint32_t, TableGlyph>*>(userData);
    auto it = table->find(cp);
    TextRenderer::GlyphLookupResult out;
    if (it != table->end()) {
        out.gm = it->second.gm;
        out.fromCjk = it->second.fromCjk;
    } else {
        out.gm = makeGm(0, 0, 0, 0, 0);  // missing glyph -> empty slot
        out.fromCjk = false;
    }
    return out;
}

} // namespace

TEST_CASE("Text layout: UTF-8 multi-byte decode emits one glyph per codepoint") {
    std::unordered_map<uint32_t, TableGlyph> table;
    table[0x41] = {makeGm(0, 0, 8, 16, 8), false};    // 'A'
    table[0x4E2D] = {makeGm(10, 0, 16, 16, 16, 0, 4), true}; // '中' (CJK)

    // "A中A" — 5 UTF-8 bytes -> 3 codepoints
    const std::string text = "A\xE4\xB8\xAD" "A";
    auto res = TextRenderer::layoutGlyphs(
        text, 10.0f, 20.0f, lookupFromTable, &table,
        true /*hasCjk*/, 1.0f / 256.0f, 1.0f / 48.0f,
        1.0f / 4096.0f, 1.0f / 4096.0f,
        false /*useTtf*/, 0.0f, 64);

    REQUIRE(res.glyphs.size() == 3);
    // pen advance = sum of advances: 8 + 16 + 8
    CHECK(res.penAdvance == doctest::Approx(42.0f));
    // first and last glyph are ASCII (not CJK), middle is CJK
    CHECK(res.glyphs[0].fromCjk == false);
    CHECK(res.glyphs[1].fromCjk == true);
    CHECK(res.glyphs[2].fromCjk == false);
    // not all CJK
    CHECK(res.allCjk == false);
}

TEST_CASE("Text layout: allCjk true only when every glyph is CJK") {
    std::unordered_map<uint32_t, TableGlyph> table;
    table[0x4E2D] = {makeGm(10, 0, 16, 16, 16, 0, 4), true};
    table[0x6587] = {makeGm(30, 0, 16, 16, 16, 0, 4), true}; // '文'

    auto res = TextRenderer::layoutGlyphs(
        "\xE4\xB8\xAD\xE6\x96\x87", 0.0f, 0.0f, lookupFromTable, &table,
        true, 1.0f / 256.0f, 1.0f / 48.0f, 1.0f / 4096.0f, 1.0f / 4096.0f,
        false, 0.0f, 64);

    REQUIRE(res.glyphs.size() == 2);
    CHECK(res.allCjk == true);
}

TEST_CASE("Text layout: empty text produces no glyphs and zero advance") {
    std::unordered_map<uint32_t, TableGlyph> table;
    auto res = TextRenderer::layoutGlyphs(
        "", 5.0f, 5.0f, lookupFromTable, &table,
        true, 1.0f / 256.0f, 1.0f / 48.0f, 1.0f / 4096.0f, 1.0f / 4096.0f,
        false, 0.0f, 64);
    CHECK(res.glyphs.empty());
    CHECK(res.penAdvance == doctest::Approx(5.0f));  // pen unchanged
    CHECK(res.allCjk == false);
}

TEST_CASE("Text layout: missing glyph emits an empty slot and still advances") {
    std::unordered_map<uint32_t, TableGlyph> table;  // no glyphs at all
    auto res = TextRenderer::layoutGlyphs(
        "A", 0.0f, 0.0f, lookupFromTable, &table,
        true, 1.0f / 256.0f, 1.0f / 48.0f, 1.0f / 4096.0f, 1.0f / 4096.0f,
        false, 0.0f, 64);
    REQUIRE(res.glyphs.size() == 1);
    CHECK(res.glyphs[0].w == 0.0f);  // empty slot
    CHECK(res.glyphs[0].h == 0.0f);
    CHECK(res.penAdvance == doctest::Approx(0.0f));  // missing glyph advance 0
    CHECK(res.allCjk == false);
}

TEST_CASE("Text layout: quad geometry uses offset, ascent and UV math") {
    std::unordered_map<uint32_t, TableGlyph> table;
    // glyph at atlas (10,5) size 8x16, advance 12, offset (2,-3)
    table[0x41] = {makeGm(10, 5, 8, 16, 12, 2, -3), false};

    const float invW = 1.0f / 256.0f;   // TTF atlas 256 wide
    const float invH = 1.0f / 48.0f;    // TTF atlas 48 tall
    auto res = TextRenderer::layoutGlyphs(
        "A", 100.0f, 200.0f, lookupFromTable, &table,
        false /*hasCjk*/, invW, invH, 1.0f / 4096.0f, 1.0f / 4096.0f,
        true /*useTtf*/, 14.0f /*ascent*/, 64);

    REQUIRE(res.glyphs.size() == 1);
    const auto& d = res.glyphs[0];
    CHECK(d.gx == doctest::Approx(100.0f + 2.0f));      // penX + offsetX
    CHECK(d.gy == doctest::Approx(200.0f - (-3.0f) + 14.0f)); // penY - offsetY + ascent
    CHECK(d.w == doctest::Approx(8.0f));
    CHECK(d.h == doctest::Approx(16.0f));
    // UVs: (10,5) -> (18,21) over atlas
    CHECK(d.u0 == doctest::Approx(10.0f / 256.0f));
    CHECK(d.v0 == doctest::Approx(5.0f / 48.0f));
    CHECK(d.u1 == doctest::Approx(18.0f / 256.0f));
    CHECK(d.v1 == doctest::Approx(21.0f / 48.0f));
    CHECK(res.penAdvance == doctest::Approx(100.0f + 12.0f));
}

TEST_CASE("Text layout: CJK glyph uses CJK UV space and default baseline") {
    std::unordered_map<uint32_t, TableGlyph> table;
    table[0x4E2D] = {makeGm(10, 5, 16, 16, 16, 1, 4), true};

    auto res = TextRenderer::layoutGlyphs(
        "\xE4\xB8\xAD", 50.0f, 60.0f, lookupFromTable, &table,
        true, 1.0f / 256.0f, 1.0f / 48.0f, 1.0f / 4096.0f, 1.0f / 4096.0f,
        true /*useTtf but glyph fromCjk*/, 14.0f, 64);

    REQUIRE(res.glyphs.size() == 1);
    const auto& d = res.glyphs[0];
    // CJK baseline: NOT the TTF ascent (8.0f default)
    CHECK(d.gy == doctest::Approx(60.0f - 4.0f + 8.0f));
    // CJK UV space uses the 4096 atlas
    CHECK(d.u0 == doctest::Approx(10.0f / 4096.0f));
    CHECK(d.v0 == doctest::Approx(5.0f / 4096.0f));
    CHECK(d.fromCjk == true);
    CHECK(res.allCjk == true);
}

TEST_CASE("Text layout: maxGlyphs truncates emitted quads, advance keeps counting") {
    std::unordered_map<uint32_t, TableGlyph> table;
    table[0x41] = {makeGm(0, 0, 8, 16, 8), false};  // 'A'

    auto res = TextRenderer::layoutGlyphs(
        "AAA", 0.0f, 0.0f, lookupFromTable, &table,
        false, 1.0f / 256.0f, 1.0f / 48.0f, 1.0f / 4096.0f, 1.0f / 4096.0f,
        false, 0.0f, /*maxGlyphs=*/2);

    REQUIRE(res.glyphs.size() == 2);            // truncated
    CHECK(res.penAdvance == doctest::Approx(24.0f));  // all 3 advances counted
}

TEST_CASE("Text layout: NDC conversion wraps pixel quads to [-1,1]") {
    std::vector<TextRenderer::LaidGlyph> glyphs;
    TextRenderer::LaidGlyph g;
    g.gx = 320.0f; g.gy = 180.0f; g.w = 10.0f; g.h = 20.0f;
    g.u0 = 0.0f; g.v0 = 0.0f; g.u1 = 1.0f; g.v1 = 1.0f;
    glyphs.push_back(g);

    std::vector<float> verts;
    std::vector<uint32_t> indices;
    TextRenderer::buildQuadVertices(glyphs, 640.0f, 360.0f, verts, indices);

    // 1 glyph -> 6 verts x 4 floats + 6 indices
    REQUIRE(verts.size() == 24);
    REQUIRE(indices.size() == 6);
    // center of screen -> NDC (0,0); quad spans 10x20 px
    // v0: (nx0, ny0, u0, v0) = ((320/640)*2-1, 1-(180/360)*2, 0, 0) = (0, 0, 0, 0)
    CHECK(verts[0] == doctest::Approx(0.0f));
    CHECK(verts[1] == doctest::Approx(0.0f));
    // v1: x1 = ((320+10)/640)*2-1 = 0.03125
    CHECK(verts[4] == doctest::Approx(0.03125f));
    // v2 = {nx1, ny1, u1, v1}: verts[8]=nx1, verts[9]=ny1, verts[10]=u1, verts[11]=v1
    // ny1 = 1-((180+20)/360)*2 = 1-1.1111 = -0.11111
    CHECK(verts[9] == doctest::Approx(-0.111111f));
    CHECK(verts[10] == doctest::Approx(1.0f));   // u1 passthrough
    CHECK(verts[11] == doctest::Approx(1.0f));   // v1 passthrough
    // indices reference 0..5
    CHECK(indices[0] == 0);
    CHECK(indices[5] == 3);
}

TEST_CASE("Text layout: NDC falls back to pixel coords when screen size is 0") {
    std::vector<TextRenderer::LaidGlyph> glyphs;
    TextRenderer::LaidGlyph g;
    g.gx = 12.0f; g.gy = 34.0f; g.w = 8.0f; g.h = 16.0f;
    glyphs.push_back(g);

    std::vector<float> verts;
    std::vector<uint32_t> indices;
    TextRenderer::buildQuadVertices(glyphs, 0.0f, 0.0f, verts, indices);

    REQUIRE(verts.size() == 24);
    // raw pixel coords pass through
    CHECK(verts[0] == doctest::Approx(12.0f));
    CHECK(verts[1] == doctest::Approx(34.0f));
    CHECK(verts[4] == doctest::Approx(20.0f));  // 12 + 8
    CHECK(verts[9] == doctest::Approx(50.0f));  // 34 + 16 (ny1)
    CHECK(verts[10] == doctest::Approx(0.0f));  // u1 = 0 (default glyph)
}

TEST_CASE("Text layout: empty glyph list produces empty vertex stream") {
    std::vector<TextRenderer::LaidGlyph> glyphs;
    std::vector<float> verts;
    std::vector<uint32_t> indices;
    TextRenderer::buildQuadVertices(glyphs, 640.0f, 360.0f, verts, indices);
    CHECK(verts.empty());
    CHECK(indices.empty());
}

TEST_CASE("TextRenderer: loadTTF handles invalid arguments and uninitialized state safely") {
    TextRenderer tr;
    // Uninitialized renderer should reject loadTTF gracefully without crashing
    CHECK_FALSE(tr.loadTTF(nullptr, 22.0f));
    CHECK_FALSE(tr.loadTTF("", 22.0f));
    CHECK_FALSE(tr.loadTTF("assets/fonts/NotoSansCJKsc-Regular.otf", 0.0f));
    CHECK_FALSE(tr.loadTTF("assets/fonts/NotoSansCJKsc-Regular.otf", -10.0f));
    CHECK_FALSE(tr.loadTTF("assets/fonts/NotoSansCJKsc-Regular.otf", 22.0f));
}