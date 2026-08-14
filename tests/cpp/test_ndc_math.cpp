// =============================================================================
// test_ndc_math.cpp — GPU-free unit tests for the shared pixel->NDC
// conversion (G8). Every CPU-side blit path (blitTexture / stretchBlt /
// affineBlt / BgfxQuadBatch::quadToNdc) delegates to NdcMath::pixelToNdc,
// so pinning this math pins the whole family.
// =============================================================================

#include "doctest.h"

#include "render/NdcMath.h"
#include "render/BgfxQuadBatch.h"   // cross-check quadToNdc delegation

using namespace Caesura;

TEST_CASE("Ndc math: screen corners map to clip-space corners") {
    // (0,0) -> (-1, 1); (sw,sh) -> (1, -1)
    auto tl = pixelToNdc(0.0f, 0.0f, 1.0f, 1.0f, 640.0f, 360.0f);
    CHECK(tl.x0 == doctest::Approx(-1.0f));
    CHECK(tl.y0 == doctest::Approx(1.0f));
    CHECK(tl.x1 == doctest::Approx(-1.0f + 2.0f / 640.0f));
    CHECK(tl.y1 == doctest::Approx(1.0f - 2.0f / 360.0f));

    auto br = pixelToNdc(639.0f, 359.0f, 1.0f, 1.0f, 640.0f, 360.0f);
    CHECK(br.x1 == doctest::Approx(1.0f));
    CHECK(br.y1 == doctest::Approx(-1.0f));
}

TEST_CASE("Ndc math: full-screen rect spans the whole clip space") {
    auto n = pixelToNdc(0.0f, 0.0f, 640.0f, 360.0f, 640.0f, 360.0f);
    CHECK(n.x0 == doctest::Approx(-1.0f));
    CHECK(n.y0 == doctest::Approx(1.0f));
    CHECK(n.x1 == doctest::Approx(1.0f));
    CHECK(n.y1 == doctest::Approx(-1.0f));
}

TEST_CASE("Ndc math: center of screen is origin") {
    auto n = pixelToNdc(320.0f, 180.0f, 0.0f, 0.0f, 640.0f, 360.0f);
    CHECK(n.x0 == doctest::Approx(0.0f));
    CHECK(n.y0 == doctest::Approx(0.0f));
}

TEST_CASE("Ndc math: y axis is flipped (pixel down = clip down)") {
    auto n = pixelToNdc(0.0f, 0.0f, 100.0f, 100.0f, 640.0f, 360.0f);
    CHECK(n.y0 > n.y1);  // top edge higher in clip space
}

TEST_CASE("Ndc math: half-screen quad at right edge stays in clip space") {
    auto n = pixelToNdc(320.0f, 0.0f, 320.0f, 360.0f, 640.0f, 360.0f);
    CHECK(n.x0 == doctest::Approx(0.0f));
    CHECK(n.x1 == doctest::Approx(1.0f));
    CHECK(n.y0 == doctest::Approx(1.0f));
    CHECK(n.y1 == doctest::Approx(-1.0f));
}

TEST_CASE("Ndc math: negative or oversized rects stay monotonic") {
    // x0 <= x1, y0 >= y1 for any input (no sign flips in the mapping).
    auto n = pixelToNdc(-100.0f, -50.0f, 900.0f, 500.0f, 640.0f, 360.0f);
    CHECK(n.x0 <= n.x1);
    CHECK(n.y0 >= n.y1);
    // The rect extends past clip space on both sides.
    CHECK(n.x0 < -1.0f);
    CHECK(n.x1 > 1.0f);
}

TEST_CASE("Ndc math: quadToNdc agrees with the shared helper") {
    // The BgfxQuadBatch wrapper must be a pure delegate: identical inputs,
    // identical outputs.
    const float x = 123.0f, y = 45.0f, w = 200.0f, h = 80.0f;
    auto shared = pixelToNdc(x, y, w, h, 1280.0f, 720.0f);
    auto wrapped = BgfxQuadBatch::quadToNdc(x, y, w, h, 1280.0f, 720.0f);
    CHECK(wrapped.nx0 == doctest::Approx(shared.x0));
    CHECK(wrapped.ny0 == doctest::Approx(shared.y0));
    CHECK(wrapped.nx1 == doctest::Approx(shared.x1));
    CHECK(wrapped.ny1 == doctest::Approx(shared.y1));
}
