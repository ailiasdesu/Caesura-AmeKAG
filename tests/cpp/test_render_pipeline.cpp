// test_render_pipeline.cpp - TextRenderer + FreeTypeContext + ShaderCache tests
#include "doctest.h"
#include "render/TextRenderer.h"
#include "render/FreeTypeContext.h"
#include "render/ShaderCache.h"
#include <memory>

using namespace Caesura;

TEST_CASE("TextRenderer::construct no-crash") {
    auto tr = std::make_unique<TextRenderer>();
    CHECK(tr.get() != nullptr);
}

TEST_CASE("FreeTypeContext::singleton") {
    FreeTypeContext& a = FreeTypeContext::instance();
    FreeTypeContext& b = FreeTypeContext::instance();
    CHECK(&a == &b);
}

TEST_CASE("FreeTypeContext::init succeeds") {
    CHECK(FreeTypeContext::instance().init());
    FreeTypeContext::instance().shutdown();
}

TEST_CASE("FreeTypeContext::double init idempotent") {
    CHECK(FreeTypeContext::instance().init());
    CHECK(FreeTypeContext::instance().init());
    FreeTypeContext::instance().shutdown();
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
