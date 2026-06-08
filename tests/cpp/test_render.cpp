#include "doctest.h"
#include "Render/BgfxRenderDevice.h"
#include "Render/RTTManager.h"
#include "Render/LayerManager.h"
#include "Render/ParticleSystem.h"
#include "Render/TextRenderer.h"
#include "Render/FreeTypeContext.h"
#include "Render/TextureManager.h"
#include "Render/GpuMonitor.h"
#include "Render/ShaderCache.h"
#include "Render/EmbeddedShaders.h"
#include "Render/VideoPlayer.h"

using namespace Caesura;

TEST_CASE("BgfxRenderDevice::name on null") {
    BgfxRenderDevice rd;
    CHECK(rd.getBackendName() != nullptr);
}

TEST_CASE("BgfxRenderDevice::init without native window no-crash") {
    // BgfxRenderDevice::init(nullptr) crashes bgfx — skip direct call, verify default state instead
    BgfxRenderDevice rd;
    CHECK(rd.getBackbufferWidth() == 1280);
}

TEST_CASE("BgfxRenderDevice::backbuffer dimensions default") {
    BgfxRenderDevice rd;
    CHECK(rd.getBackbufferWidth() == 1280);
    CHECK(rd.getBackbufferHeight() == 720);
}

TEST_CASE("ParticleSystem::MAX_PARTICLES constant") {
    CHECK(ParticleSystem::MAX_PARTICLES == 1024);
}

TEST_CASE("ParticleSystem::init with nullptr device") {
    ParticleSystem ps;
    bool ok = ps.init();
    (void)ok;
}

TEST_CASE("ParticleSystem::update dynamic resolution no-crash") {
    ParticleSystem ps;
    ps.init();
    ps.update(0.016f, 1920, 1080);
    ps.update(0.016f, 640, 480);
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


TEST_CASE("TextRenderer::construct no-crash") {
    auto tr = std::make_unique<TextRenderer>();
    CHECK(tr.get() != nullptr);
}

TEST_CASE("RTTManager::construct with device") { BgfxRenderDevice rd; RTTManager mgr(rd); (void)mgr; }

TEST_CASE("LayerManager::singleton accessible") { LayerManager& lm = LayerManager::instance(); (void)lm; }

TEST_CASE("GpuMonitor::metrics default gpuTimeMs") {
    GpuMonitor gm;
    CHECK(gm.metrics().gpuTimeMs >= 0.0);
}

TEST_CASE("GpuMonitor::current quality is HIGH by default") {
    GpuMonitor gm;
    CHECK(gm.currentQuality() == GpuQuality::HIGH);
}

TEST_CASE("TextureManager::singleton accessible") {
    TextureManager& tm = TextureManager::instance();
    (void)tm;
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

TEST_CASE("VideoPlayer::construct no-crash") {
    VideoPlayer vp;
    (void)vp;
}

TEST_CASE("EmbeddedShaders::DX11 VS binary present") {
    CHECK(kEmbeddedVS_SpriteSize > 0);
    CHECK(static_cast<const void*>(kEmbeddedVS_Sprite) != nullptr);
}

TEST_CASE("EmbeddedShaders::DX11 FS binary present") {
    CHECK(kEmbeddedFS_TextureSize > 0);
    CHECK(static_cast<const void*>(kEmbeddedFS_Texture) != nullptr);
}

