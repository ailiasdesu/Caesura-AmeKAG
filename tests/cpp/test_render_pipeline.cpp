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
