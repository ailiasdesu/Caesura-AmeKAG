// =============================================================================
// test_color_filter.cpp — GPU-free unit tests for the accessibility color
// filter math (G8). Pins the Machado-2009 preset matrices and the effect-4
// VFX uniform packing shared by BgfxRenderDevice and BgfxDraw.
// =============================================================================

#include "doctest.h"

#include "render/ColorFilterMath.h"

using namespace Caesura;
using PF = IRenderDevice::ColorFilterPreset;

TEST_CASE("Color filter: None preset returns nullptr") {
    CHECK(colorFilterPresetMatrix(PF::None) == nullptr);
}

TEST_CASE("Color filter: every preset returns a valid 3x3 matrix") {
    const PF presets[] = {
        PF::Deuteranopia, PF::Protanopia, PF::Tritanopia,
        PF::Grayscale, PF::HighContrast,
    };
    for (auto p : presets) {
        const float* m = colorFilterPresetMatrix(p);
        REQUIRE(m != nullptr);
        // All finite (no NaN/Inf) and non-degenerate.
        for (int i = 0; i < 9; ++i) {
            CHECK(m[i] == m[i]);              // not NaN
            CHECK(m[i] > -10.0f);
            CHECK(m[i] < 10.0f);
        }
    }
}

TEST_CASE("Color filter: deuteranopia matrix matches Machado 2009 reference") {
    const float* m = colorFilterPresetMatrix(PF::Deuteranopia);
    REQUIRE(m != nullptr);
    CHECK(m[0] == doctest::Approx(0.367f));
    CHECK(m[1] == doctest::Approx(0.861f));
    CHECK(m[2] == doctest::Approx(-0.228f));
    CHECK(m[3] == doctest::Approx(0.280f));
    CHECK(m[4] == doctest::Approx(0.673f));
    CHECK(m[5] == doctest::Approx(0.047f));
    CHECK(m[6] == doctest::Approx(-0.012f));
    CHECK(m[7] == doctest::Approx(0.043f));
    CHECK(m[8] == doctest::Approx(0.969f));
}

TEST_CASE("Color filter: grayscale rows are luminance weights") {
    const float* m = colorFilterPresetMatrix(PF::Grayscale);
    REQUIRE(m != nullptr);
    // Classic Rec.601 luminance weights, identical in every row.
    for (int row = 0; row < 3; ++row) {
        const float r = m[row * 3 + 0], g = m[row * 3 + 1], b = m[row * 3 + 2];
        CHECK(r == doctest::Approx(0.299f));
        CHECK(g == doctest::Approx(0.587f));
        CHECK(b == doctest::Approx(0.114f));
        CHECK(r + g + b == doctest::Approx(1.0f).epsilon(0.01f));
    }
}

TEST_CASE("Color filter: high contrast is a 1.25x scale") {
    const float* m = colorFilterPresetMatrix(PF::HighContrast);
    REQUIRE(m != nullptr);
    CHECK(m[0] == doctest::Approx(1.25f));
    CHECK(m[4] == doctest::Approx(1.25f));
    CHECK(m[8] == doctest::Approx(1.25f));
    CHECK(m[1] == 0.0f);
    CHECK(m[2] == 0.0f);
    CHECK(m[3] == 0.0f);
    CHECK(m[5] == 0.0f);
    CHECK(m[6] == 0.0f);
    CHECK(m[7] == 0.0f);
}

TEST_CASE("Color filter: pack spreads matrix across VFXParams layout") {
    const float matrix[9] = {
        1.0f, 2.0f, 3.0f,
        4.0f, 5.0f, 6.0f,
        7.0f, 8.0f, 9.0f,
    };
    const float fadeAlpha = 0.5f;
    const auto p = packVfxColorFilter(matrix, fadeAlpha);

    // m0 = color.rgb (rows 0), alpha stays fadeAlpha.
    CHECK(p.color[0] == doctest::Approx(1.0f));
    CHECK(p.color[1] == doctest::Approx(2.0f));
    CHECK(p.color[2] == doctest::Approx(3.0f));
    CHECK(p.color[3] == doctest::Approx(0.5f));

    // m1 = (blurQuake.x, 0, blurQuake.z, blurQuake.w) = row 1.
    CHECK(p.blurQuake[0] == doctest::Approx(4.0f));
    CHECK(p.blurQuake[1] == 0.0f);
    CHECK(p.blurQuake[2] == doctest::Approx(5.0f));
    CHECK(p.blurQuake[3] == doctest::Approx(6.0f));

    // m2 = padding.xyz = row 2.
    CHECK(p.padding[0] == doctest::Approx(7.0f));
    CHECK(p.padding[1] == doctest::Approx(8.0f));
    CHECK(p.padding[2] == doctest::Approx(9.0f));
}

TEST_CASE("Color filter: pack round-trips a real preset") {
    const float* m = colorFilterPresetMatrix(PF::Deuteranopia);
    REQUIRE(m != nullptr);
    const auto p = packVfxColorFilter(m, 0.75f);
    // identity mapping: m[i] == packed fields, alpha preserved
    CHECK(p.color[0] == doctest::Approx(m[0]));
    CHECK(p.color[3] == doctest::Approx(0.75f));
    CHECK(p.blurQuake[0] == doctest::Approx(m[3]));
    CHECK(p.blurQuake[1] == 0.0f);
    CHECK(p.padding[2] == doctest::Approx(m[8]));
}