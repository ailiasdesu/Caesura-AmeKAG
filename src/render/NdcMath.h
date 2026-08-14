#pragma once

// NdcMath.h - Pure pixel->clip-space conversion shared by every CPU-side
// blit path (blitTexture / stretchBlt / affineBlt / BgfxQuadBatch). The
// fallback vertex shader is passthrough (gl_Position = vec4(a_position,0,1)),
// so pixel coords (0..w, 0..h) must be converted to NDC [-1,1] on the CPU;
// without it the whole quad lies outside clip space and is culled.
//
// Header-only and GPU-free: unit tests exercise this math directly without
// a window or a bgfx context (P2-10 / G8).

namespace Caesura {

struct NdcRect {
    float x0, y0;  // top-left in clip space
    float x1, y1;  // bottom-right in clip space
};

// Convert a pixel-space rect to clip space for a backbuffer of
// screenW x screenH. Callers must guard screenW/screenH > 0 (a zero-size
// backbuffer would produce infinite coordinates).
inline NdcRect pixelToNdc(float x, float y, float w, float h,
                          float screenW, float screenH) {
    NdcRect r;
    r.x0 = (x / screenW) * 2.0f - 1.0f;
    r.y0 = 1.0f - (y / screenH) * 2.0f;
    r.x1 = ((x + w) / screenW) * 2.0f - 1.0f;
    r.y1 = 1.0f - ((y + h) / screenH) * 2.0f;
    return r;
}

} // namespace Caesura
