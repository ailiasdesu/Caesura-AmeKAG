// =============================================================================
// test_render_postfx.cpp - GPU-free contract tests for the round-102
// post-processing chain API (IRenderDevice::PostFx*).
//
// The postfx chain lives behind IRenderDevice (full-screen passes applied to
// the scene before backbuffer composite). This file pins the CONTRACT:
//   (a) PostFxKind enum values / PostFxParams defaults / PostFxHandle type;
//   (b) NullRenderDevice graceful-degradation semantics (unsupported ->
//       isPostFxSupported=false, createPostFx returns 0, everything else
//       is a safe no-op);
//   (c) BgfxRenderDevice gate: isPostFxSupported requires a GPU-initialized
//       device, so on a freshly default-constructed device createPostFx
//       returns 0 (graceful). The *real* handle lifecycle (stable handles =
//       index+1, destroy/clear reordering) is only reachable after
//       init(nativeWindow,...) succeeds, which needs a GPU - it is therefore
//       NOT covered here and must be validated on a real device unless a
//       headless bgfx software/passthrough renderer is introduced.
//
// See docs/solutions/deferred-gpu-tests.md for the engine-wide convention on
// GPU-dependent test coverage.
// =============================================================================

#include "doctest.h"

#include "render/api/IRenderDevice.h"
#include "render/BgfxRenderDevice.h"
#include "render/NullRenderDevice.h"

#include <cstdint>
#include <type_traits>

using namespace Caesura;

// -----------------------------------------------------------------------------
// (a) Interface existence / signature (compile-time contract pins).
// -----------------------------------------------------------------------------

TEST_CASE("postfx: PostFxKind enum values are fixed") {
    CHECK(static_cast<int>(IRenderDevice::PostFxKind::Vignette) == 0);
    CHECK(static_cast<int>(IRenderDevice::PostFxKind::LutColorGrade) == 1);
    CHECK(static_cast<int>(IRenderDevice::PostFxKind::SoftBlur) == 2);
    CHECK(static_cast<int>(IRenderDevice::PostFxKind::Bloom) == 3);
    CHECK(static_cast<int>(IRenderDevice::PostFxKind::Lut3D) == 4); // t214
}

TEST_CASE("postfx: PostFxHandle is a uint32_t") {
    static_assert(std::is_same<IRenderDevice::PostFxHandle, uint32_t>::value,
                  "PostFxHandle must be uint32_t");
    // 0 is the documented "invalid/unsupported" sentinel.
    CHECK(IRenderDevice::PostFxHandle(0) == 0);
}

TEST_CASE("postfx: PostFxParams defaults match the contract") {
    IRenderDevice::PostFxParams p;
    CHECK(p.strength == doctest::Approx(1.0f));
    CHECK(p.radius == doctest::Approx(0.0f));
    CHECK(p.amount == doctest::Approx(0.0f));
    CHECK(p.r == doctest::Approx(1.0f));
    CHECK(p.g == doctest::Approx(1.0f));
    CHECK(p.b == doctest::Approx(1.0f));
    CHECK(p.lutMix == doctest::Approx(0.0f));
    // t214: Lut3D fields default to "no texture" (borrowed semantics: the
    // TextureManager stays the owner; the stage only references it).
    CHECK_FALSE(p.lutTexture.isValid());
    CHECK(p.lutSize == 0);
}

TEST_CASE("postfx: every PostFx method surface exists (compile-time)") {
    // Pin each virtual signature through the abstract interface so a
    // broken signature (arg order / const-ness / return type) fails to build.
    IRenderDevice::PostFxParams p;
    (void)static_cast<bool (IRenderDevice::*)(IRenderDevice::PostFxKind) const>(&IRenderDevice::isPostFxSupported);
    (void)static_cast<IRenderDevice::PostFxHandle (IRenderDevice::*)(IRenderDevice::PostFxKind, const IRenderDevice::PostFxParams&)>(&IRenderDevice::createPostFx);
    (void)static_cast<void (IRenderDevice::*)(IRenderDevice::PostFxHandle, const IRenderDevice::PostFxParams&)>(&IRenderDevice::setPostFxParams);
    (void)static_cast<void (IRenderDevice::*)(IRenderDevice::PostFxHandle)>(&IRenderDevice::destroyPostFx);
    (void)static_cast<void (IRenderDevice::*)()>(&IRenderDevice::clearPostFx);
    (void)static_cast<bool (IRenderDevice::*)() const>(&IRenderDevice::isPostFxActive);
    (void)p;
}

// -----------------------------------------------------------------------------
// (b) NullRenderDevice graceful degradation (fully headless / contract-faithful).
// -----------------------------------------------------------------------------

TEST_CASE("Null postfx: all kinds unsupported") {
    NullRenderDevice dev;
    for (int k = 0; k <= 4; ++k) { // t214: Lut3D=4 degrades like the rest
        auto kind = static_cast<IRenderDevice::PostFxKind>(k);
        CHECK_FALSE(dev.isPostFxSupported(kind));
    }
}

TEST_CASE("Null postfx: createPostFx returns invalid handle (0)") {
    NullRenderDevice dev;
    IRenderDevice::PostFxParams p;
    for (int k = 0; k <= 4; ++k) { // t214: Lut3D=4 included
        auto kind = static_cast<IRenderDevice::PostFxKind>(k);
        CHECK(dev.createPostFx(kind, p) == 0);
    }
}

TEST_CASE("Null postfx: isPostFxActive always false") {
    NullRenderDevice dev;
    CHECK_FALSE(dev.isPostFxActive());
}

TEST_CASE("Null postfx: all methods are safe no-ops (no crash)") {
    NullRenderDevice dev;
    IRenderDevice::PostFxParams p;
    // Set params on an invalid (0) handle.
    CHECK_NOTHROW(dev.setPostFxParams(0, p));
    // Set params on a non-zero handle that was never created.
    CHECK_NOTHROW(dev.setPostFxParams(42, p));
    // Destroy invalid / unknown handles.
    CHECK_NOTHROW(dev.destroyPostFx(0));
    CHECK_NOTHROW(dev.destroyPostFx(1));
    CHECK_NOTHROW(dev.destroyPostFx(0xFFFFFFFFu));
    // Clear the (empty) chain.
    CHECK_NOTHROW(dev.clearPostFx());
    CHECK_NOTHROW(dev.clearPostFx());
    // Repeatable / order-independent - a Null device must stay inert.
    CHECK_FALSE(dev.isPostFxActive());
}

// -----------------------------------------------------------------------------
// (c) BgfxRenderDevice gate (documented limitation - no GPU in CI).
// -----------------------------------------------------------------------------
// isPostFxSupported returns `m_bgfxInitialized && m_shaders != nullptr`, both
// of which are unset until init(nativeWindow,...) succeeds on a real GPU. On a
// default-constructed device createPostFx is therefore gated to 0. The actual
// handle lifecycle (stable handle = index+1, destroy/clear renumbering) is
// exercised only after a successful GPU init and is out of headless scope.

TEST_CASE("Bgfx postfx: unsupported without GPU init (graceful 0)") {
    BgfxRenderDevice dev;
    IRenderDevice::PostFxParams p;
    CHECK_FALSE(dev.isPostFxSupported(IRenderDevice::PostFxKind::Vignette));
    CHECK_FALSE(dev.isPostFxSupported(IRenderDevice::PostFxKind::Bloom));
    CHECK(dev.createPostFx(IRenderDevice::PostFxKind::Vignette, p) == 0);
    CHECK_FALSE(dev.isPostFxActive());
    // The full method surface stays safe even with an empty chain.
    CHECK_NOTHROW(dev.setPostFxParams(0, p));
    CHECK_NOTHROW(dev.clearPostFx());
}

// NOTE (deferred to GPU): handle lifecycle contract -
//   createPostFx -> handle = m_postFxStages.size() (1-based sequence)
//   setPostFxParams(handle>size|0) is a no-op
//   destroyPostFx(handle>size|0) is a no-op; valid destroys renumber
//   clearPostFx() empties the chain; isPostFxActive() reflects non-empty
// This behavior depends on a live initialized BgfxRenderDevice.