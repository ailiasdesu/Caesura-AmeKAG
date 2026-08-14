#pragma once

// ColorFilterMath.h - Pure accessibility color-filter math (G8).
//
// The preset matrices (Machado, Oliveira & Fernandes 2009, "A Physiologically-
// based Model for Simulating Color Vision Deficiency") and the effect-4 VFX
// uniform packing are extracted here so they can be pinned by GPU-free unit
// tests and shared by BgfxRenderDevice (preset lookup) and BgfxDraw
// (VFXParams packing).
//
// Header-only, no bgfx dependency.

#include "api/IRenderDevice.h"   // ColorFilterPreset enum
#include <cstddef>

namespace Caesura {

// Row-major 3x3 matrix for a color filter preset, or nullptr for None.
// The returned pointer is valid for the process lifetime.
const float* colorFilterPresetMatrix(IRenderDevice::ColorFilterPreset preset);

// Effect-4 VFX uniform packing: the active preset matrix is spread across the
// VFXParams layout (m0 = color.rgb, m1.x = blur, m1.z/w = quake.yz are reused
// as matrix rows 1-2; m2 = padding). fadeAlpha stays in color[3].
struct VfxColorFilterPack {
    float color[4];
    float blurQuake[4];
    float padding[3];
};
// `matrix` must point to 9 floats (row-major 3x3). Returns the pack.
VfxColorFilterPack packVfxColorFilter(const float* matrix, float fadeAlpha);

} // namespace Caesura
