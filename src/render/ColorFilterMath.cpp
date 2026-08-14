#include "ColorFilterMath.h"

namespace Caesura {

// Accessibility color filter presets (Machado et al. 2009 matrices):
// row-major 3x3, applied by effect-4 VFX passes (see BgfxDraw_Effects).
const float* colorFilterPresetMatrix(IRenderDevice::ColorFilterPreset preset) {
    static const float kDeuteranopia[9] = {
        0.367f, 0.861f, -0.228f,
        0.280f, 0.673f,  0.047f,
        -0.012f, 0.043f,  0.969f,
    };
    static const float kProtanopia[9] = {
        0.152f, 1.053f, -0.205f,
        0.115f, 0.786f,  0.099f,
        -0.004f, 0.028f,  0.976f,
    };
    static const float kTritanopia[9] = {
        1.256f, -0.077f, -0.179f,
        -0.078f, 0.931f,  0.148f,
        0.005f,  0.691f,  0.304f,
    };
    static const float kGrayscale[9] = {
        0.299f, 0.587f, 0.114f,
        0.299f, 0.587f, 0.114f,
        0.299f, 0.587f, 0.114f,
    };
    static const float kHighContrast[9] = {
        1.25f, 0.0f,  0.0f,
        0.0f,  1.25f, 0.0f,
        0.0f,  0.0f,  1.25f,
    };
    switch (preset) {
    case IRenderDevice::ColorFilterPreset::None:         return nullptr;
    case IRenderDevice::ColorFilterPreset::Deuteranopia: return kDeuteranopia;
    case IRenderDevice::ColorFilterPreset::Protanopia:   return kProtanopia;
    case IRenderDevice::ColorFilterPreset::Tritanopia:   return kTritanopia;
    case IRenderDevice::ColorFilterPreset::Grayscale:    return kGrayscale;
    case IRenderDevice::ColorFilterPreset::HighContrast: return kHighContrast;
    }
    return nullptr;
}

VfxColorFilterPack packVfxColorFilter(const float* matrix, float fadeAlpha) {
    VfxColorFilterPack out{};
    // Effect 4 (colorblind/contrast filter): m0 = color.rgb, m1 =
    // (blurQuake.x, blurQuake.z, blurQuake.w), m2 = padding.xyz.
    out.color[0] = matrix[0]; out.color[1] = matrix[1];
    out.color[2] = matrix[2]; out.color[3] = fadeAlpha;
    out.blurQuake[0] = matrix[3]; out.blurQuake[1] = 0.0f;
    out.blurQuake[2] = matrix[4]; out.blurQuake[3] = matrix[5];
    out.padding[0] = matrix[6]; out.padding[1] = matrix[7];
    out.padding[2] = matrix[8];
    return out;
}

} // namespace Caesura
