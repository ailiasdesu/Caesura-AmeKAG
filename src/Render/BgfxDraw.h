#pragma once

#include <bgfx/bgfx.h>
#include <vector>
#include <cstdint>

namespace Caesura {

class BgfxShaderManager;
class BgfxDeviceCore;

// BgfxDraw — batch quad rendering, texture blit, stretch/affine, and fullscreen effects.
// Extracted from BgfxRenderDevice (U7 step 3).

class BgfxDraw {
public:
    BgfxDraw() = default;
    ~BgfxDraw() = default;

    BgfxDraw(const BgfxDraw&) = delete;
    BgfxDraw& operator=(const BgfxDraw&) = delete;

    void init(BgfxShaderManager* shaders, BgfxDeviceCore* device);
    void flushAllRTT();

    void beginBatch();
    void flushBatch();

    void blitTexture(uint16_t targetView, uint32_t textureId, float x, float y, float w, float h, uint8_t opacity);
    void blitTexture(uint16_t targetView, bgfx::TextureHandle tex, float x, float y, float w, float h, uint8_t opacity);
    void blitViewport(uint32_t handle, uint16_t targetView, float x, float y, float w, float h);
    void fillViewport(uint32_t handle, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    void stretchBlt(uint16_t targetView, uint32_t dstTexId, float dx, float dy, float dw, float dh,
                    uint32_t srcTexId, float sx, float sy, float sw, float sh, int filterType);
    void affineBlt(uint16_t targetView, uint32_t dstTexId, float dx, float dy, float dw, float dh,
                   uint32_t srcTexId, float sx, float sy, float sw, float sh, const float matrix[6]);

    void submitBlend(uint16_t viewId, bgfx::TextureHandle baseTex, bgfx::TextureHandle blendTex,
                     int mode, float baseAlpha, float blendAlpha, float globalAlpha);
    void submitTransition(uint16_t viewId, bgfx::TextureHandle fromTex, bgfx::TextureHandle toTex,
                          bgfx::TextureHandle ruleTex, int method, float progress);
    void submitVFX(uint16_t viewId, bgfx::TextureHandle srcTex, int effect,
                   float fadeAlpha, float fadeR, float fadeG, float fadeB,
                   float blurRadius, float quakeX, float quakeY);

private:
    BgfxShaderManager* m_shaders = nullptr;
    BgfxDeviceCore*    m_device  = nullptr;

    struct BatchQuad {
        uint16_t viewId;
        bgfx::TextureHandle tex;
        float x, y, w, h;
        uint8_t opacity;
    };
    bool m_batching = false;
    std::vector<BatchQuad> m_batchQuads;
};

} // namespace Caesura
