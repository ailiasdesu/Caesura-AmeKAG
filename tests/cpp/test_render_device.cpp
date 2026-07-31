// test_render_device.cpp - BgfxRenderDevice + RTTManager + EmbeddedShaders tests
#include "doctest.h"
#include "render/BgfxRenderDevice.h"
#include "render/RTTManager.h"
#include "render/EmbeddedShaders.h"
#include "render/NullRenderDevice.h"

using namespace Caesura;

TEST_CASE("BgfxRenderDevice::name on null") {
    BgfxRenderDevice rd;
    CHECK(rd.getBackendName() != nullptr);
}

TEST_CASE("BgfxRenderDevice::backbuffer dimensions default") {
    BgfxRenderDevice rd;
    CHECK(rd.getBackbufferWidth() == 1280);
    CHECK(rd.getBackbufferHeight() == 720);
}

TEST_CASE("BgfxRenderDevice::shutdown without init is safe") {
    BgfxRenderDevice rd;
    rd.shutdown();

    BgfxDeviceCore core;
    core.shutdown();
    core.shutdown();
}

TEST_CASE("BgfxRenderDevice::double shutdown is idempotent") {
    BgfxRenderDevice rd;
    rd.shutdown();
    rd.shutdown();
}

TEST_CASE("Bgfx frame entry points are safe outside an initialized lifetime") {
    BgfxRenderDevice rd;
    CHECK_NOTHROW(rd.beginFrame());
    CHECK_NOTHROW(rd.endFrame());
    CHECK_NOTHROW(rd.commit_frame());
    CHECK_NOTHROW(rd.advanceFrame());

    rd.shutdown();
    CHECK_NOTHROW(rd.beginFrame());
    CHECK_NOTHROW(rd.endFrame());
    CHECK_NOTHROW(rd.commit_frame());
    CHECK_NOTHROW(rd.advanceFrame());

    BgfxDeviceCore core;
    CHECK_NOTHROW(core.beginFrame());
    CHECK_NOTHROW(core.endFrame());
    CHECK_NOTHROW(core.commit_frame());
}

TEST_CASE("RTTManager::construct with device") {
    BgfxRenderDevice rd;
    RTTManager mgr(rd);
    (void)mgr;
}

TEST_CASE("EmbeddedShaders::DX11 VS binary present") {
    CHECK(kEmbeddedVS_SpriteSize > 0);
    CHECK(static_cast<const void*>(kEmbeddedVS_Sprite) != nullptr);
}

TEST_CASE("EmbeddedShaders::DX11 FS binary present") {
    CHECK(kEmbeddedFS_TextureSize > 0);
    CHECK(static_cast<const void*>(kEmbeddedFS_Texture) != nullptr);
}

TEST_CASE("Render: TTF load rejects missing file without GPU") {
    BgfxRenderDevice rd;
    // loadTTF performs FreeType initialization and file open before any
    // bgfx texture upload; a missing path must fail cleanly.
    CHECK_FALSE(rd.loadTTF("__missing_font__.ttf", 24.0f));
}

TEST_CASE("Render: NullRenderDevice loadTTF is a safe no-op") {
    NullRenderDevice rd;
    CHECK_FALSE(rd.loadTTF("any.ttf", 24.0f));
}