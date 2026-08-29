// test_render_pipeline.cpp - TextRenderer + ShaderCache tests
#include "doctest.h"
#include "render/TextRenderer.h"
#include "render/ShaderCache.h"
#include <filesystem>
#include <memory>

using namespace Caesura;

TEST_CASE("TextRenderer::construct no-crash") {
    auto tr = std::make_unique<TextRenderer>();
    CHECK(tr.get() != nullptr);
}

TEST_CASE("TextRenderer::default state uses bitmap font") {
    TextRenderer tr;
    CHECK_FALSE(tr.isInitialized());
    CHECK(tr.currentFont() == FontId::Small);
    CHECK(tr.lineHeight() == doctest::Approx(16.0f));
}

TEST_CASE("TextRenderer::init rejects null device") {
    TextRenderer tr;
    CHECK_FALSE(tr.init(nullptr));
    CHECK_FALSE(tr.isInitialized());
}

TEST_CASE("TextRenderer::missing TTF fails repeatedly") {
    TextRenderer tr;
    constexpr const char* missingFont = "__caesura_missing_font_for_raii_test__.ttf";
    CHECK_FALSE(tr.loadTTF(missingFont));
    CHECK_FALSE(tr.loadTTF(missingFont));
    CHECK(tr.currentFont() == FontId::Small);
}

TEST_CASE("TextRenderer::loadTTF refuses without GPU context") {
    // R7 edge case: loadTTF ends with a bgfx atlas upload, so calling it
    // before bgfx::init is UB. A real font file on disk must be refused
    // gracefully (no crash) while the GPU is not initialized.
    TextRenderer tr;
    const char* candidates[] = {
        "../../../assets/fonts/NotoSansCJKsc-Regular.otf",
        "assets/fonts/NotoSansCJKsc-Regular.otf",
        "../../assets/fonts/NotoSansCJKsc-Regular.otf",
    };
    const char* realFont = nullptr;
    for (const char* c : candidates) {
        if (std::filesystem::exists(c)) { realFont = c; break; }
    }
    if (!realFont) {
        MESSAGE("CJK font asset not present; skipping GPU-guard check");
        return;
    }
    CHECK_FALSE(tr.loadTTF(realFont, 24.0f));
    CHECK(tr.currentFont() == FontId::Small);
    CHECK(tr.lineHeight() == doctest::Approx(16.0f));
}

TEST_CASE("ShaderCache::default empty") {
    CompositeShaderCache& cache = CompositeShaderCache::instance();
    CHECK(cache.size() == 0);
    CHECK(cache.maxSize() == 64);
}

TEST_CASE("ShaderCache::evict oldest when full") {
    CompositeShaderCache& cache = CompositeShaderCache::instance();
    for (int i = 0; i < 65; i++) {
        CompositeShaderKey key;
        key.blendMode = i % 28;
        bgfx::ProgramHandle ph = BGFX_INVALID_HANDLE;
        cache.registerProgram(key, ph);
    }
    CHECK(cache.size() <= 64);
}

TEST_CASE("ShaderCache::legal unregistered blend modes alias the non-palette program (t86)") {
    // t86 regression: precompileCommon asks for Difference(10)/Exclusion(11)/
    // Add(16) without palette; the engine individually registers a subset
    // (Normal..SoftLight 0-9), so these are legal keys that are never
    // individually registered -- by design they resolve to the SAME
    // non-palette program (the blend mode travels as a uniform). The unregistered
    // path must NOT be taken for legal enums (the round-3 / release-verify
    // '[ERROR] unregistered variant blend=10/11' noise), and out-of-range keys
    // keep the loud documented degrade.
    CompositeShaderCache& cache = CompositeShaderCache::instance();
    CompositeShaderKey normalKey;
    normalKey.blendMode = static_cast<int>(BlendMode::Normal);
    bgfx::ProgramHandle ph{ 1 };    // idx-valid handle: bgfx::isValid is idx-based
    cache.registerProgram(normalKey, ph);
    CHECK(cache.size() >= 1);

    // t90 discriminator: the t86-era handle-equality check alone could not tell
    // the aliased path apart from the OLD unregistered path -- both returned the
    // SAME handle (only the log level differed, ERROR vs INFO, and doctest has
    // no log capture). The alias-path counter is the observable surface: every
    // legal first-time lookup resolves through the alias branch and increments
    // it exactly once; the old code never increments it. RED under the old code.
    const size_t aliasBefore = cache.aliasedVariantCount();
    for (int mode : { (int)BlendMode::Difference,   // 10
                      (int)BlendMode::Exclusion,    // 11
                      (int)BlendMode::Add }) {      // 16
        CompositeShaderKey key;
        key.blendMode = mode;
        CHECK(cache.getProgram(key).idx == ph.idx);
    }
    CHECK(cache.aliasedVariantCount() == aliasBefore + 3);

    // Out-of-range stays on the diagnostic degrade (Normal fallback, loud log).
    CompositeShaderKey bad;
    bad.blendMode = static_cast<int>(BlendMode::COUNT) + 3;
    CHECK(cache.getProgram(bad).idx == ph.idx);
    // t90: out-of-range is NOT a legal blend mode -- the alias branch is gated
    // by the legal range, so the counter does not advance here (the degrade
    // path is still exercised and must remain observable).
    CHECK(cache.aliasedVariantCount() == aliasBefore + 3);
}

TEST_CASE("TextRenderer::glyphQuadToNDC italic shears the top edge only") {
    // 1280x720 ortho; pixel (px,py) -> NDC ((px/1280)*2-1, 1-(py/720)*2)
    const float sw = 1280.0f, sh = 720.0f;

    // Upright quad: vertical edges stay vertical, all x = pixel x.
    auto u = TextRenderer::glyphQuadToNDC(100.f, 200.f, 16.f, 16.f, 0.f, sw, sh);
    CHECK(u.x0 == doctest::Approx((100.f / sw) * 2.f - 1.f));
    CHECK(u.x1 == doctest::Approx((116.f / sw) * 2.f - 1.f));
    CHECK(u.x0 == u.x3);  // left edge vertical
    CHECK(u.x1 == u.x2);  // right edge vertical
    CHECK(u.y0 == doctest::Approx(1.f - (200.f / sh) * 2.f));
    CHECK(u.y1 == u.y0);  // top edge horizontal
    CHECK(u.y2 == doctest::Approx(1.f - (216.f / sh) * 2.f));
    CHECK(u.y3 == u.y2);  // bottom edge horizontal

    // Italic: top edge shifted right by shear; bottom edge fixed.
    auto v = TextRenderer::glyphQuadToNDC(100.f, 200.f, 16.f, 16.f, 3.f, sw, sh);
    CHECK(v.x0 == doctest::Approx((103.f / sw) * 2.f - 1.f));  // top-left
    CHECK(v.x1 == doctest::Approx((119.f / sw) * 2.f - 1.f));  // top-right
    CHECK(v.x2 == doctest::Approx((116.f / sw) * 2.f - 1.f));  // bottom-right FIXED
    CHECK(v.x3 == doctest::Approx((100.f / sw) * 2.f - 1.f));  // bottom-left FIXED
    // The top edge must slant relative to the bottom edge, not translate.
    CHECK(v.x0 > u.x0);
    CHECK(v.x1 > u.x1);
    CHECK(v.x2 == doctest::Approx(u.x2));  // bottom edge unchanged
    CHECK(v.x3 == doctest::Approx(u.x3));
    CHECK(v.y0 == doctest::Approx(u.y0));  // y coordinates unchanged
    CHECK(v.y2 == doctest::Approx(u.y2));

    // Advance metrics are unaffected: width spans bottom edge only.
    CHECK(v.x2 - v.x3 == doctest::Approx(u.x2 - u.x3));
}

TEST_CASE("TextRenderer::strike bar geometry crosses the glyph middle") {
    const float sw = 1280.0f, sh = 720.0f;
    // A strike bar is an axis-aligned quad at y + 0.5*h with 10% height
    // (reuses glyphQuadToNDC with shear 0). Anchor the exact bar math
    // used by renderText so geometry regressions surface headless.
    const float x = 100.f, y = 200.f, w = 16.f, h = 16.f;
    const float barH = 0.1f * h;                     // 1.6 (>= 1.0 floor)
    const float barY = y + h * 0.5f;               // 208
    auto v = TextRenderer::glyphQuadToNDC(x, barY, w, barH, 0.f, sw, sh);
    CHECK(v.x0 == doctest::Approx((100.f / sw) * 2.f - 1.f));
    CHECK(v.x1 == doctest::Approx((116.f / sw) * 2.f - 1.f));
    CHECK(v.y0 == doctest::Approx(1.f - (208.f / sh) * 2.f));  // top edge
    CHECK(v.y2 == doctest::Approx(1.f - (209.6f / sh) * 2.f)); // bottom edge
    // NDC y grows upward: height is y0 - y2 (positive downward extent).
    CHECK(v.y0 - v.y2 == doctest::Approx((barH / sh) * 2.f));
    // Bar spans the full glyph advance width.
    CHECK(v.x1 - v.x0 == doctest::Approx((w / sw) * 2.f));
    // Vertical placement: bar TOP sits at the glyph's vertical middle
    // (y + 0.5*h), so the bar crosses the center of the glyph.
    auto g = TextRenderer::glyphQuadToNDC(x, y, w, h, 0.f, sw, sh);
    CHECK(v.y0 == doctest::Approx((g.y0 + g.y2) / 2.f));
}
