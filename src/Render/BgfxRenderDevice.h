#pragma once

#include "IRenderDevice.h"
#include "EmbeddedShaders.h"
#include "TextRenderer.h"
#include <memory>
#include <bgfx/bgfx.h>
#include <unordered_map>
#include <memory>
#include <string>

namespace Caesura {

class BgfxRenderDevice final : public IRenderDevice {
public:
    BgfxRenderDevice() = default;
    ~BgfxRenderDevice() override;

    BgfxRenderDevice(const BgfxRenderDevice&) = delete;
    BgfxRenderDevice& operator=(const BgfxRenderDevice&) = delete;

    static bool setPreferredBackend(const char* name);
    const char* getBackendName() const;

    bool init(void* nativeWindowHandle, int width, int height) override;
    void resize(int width, int height) override;
    void shutdown() override;
    void beginFrame() override;
    void endFrame() override;
    void commit_frame() override;
    void setViewRect(uint16_t viewId, uint16_t x, uint16_t y,
                     uint16_t width, uint16_t height) override;
    void setViewClear(uint16_t viewId, uint16_t flags,
                      uint32_t rgba, float depth, uint8_t stencil) override;
    void touch(uint16_t viewId) override;
    ViewportHandle createRenderTarget(int width, int height) override;
    void destroyRenderTarget(ViewportHandle handle) override;
    void blitTexture(uint16_t targetView, uint32_t textureId,
                      float x, float y, float w, float h, uint8_t opacity) override;
    void blitTexture(uint16_t targetView, bgfx::TextureHandle tex,
                      float x, float y, float w, float h, uint8_t opacity);
    void blitViewport(ViewportHandle handle, uint16_t targetView,
                      float x, float y, float w, float h) override;
    bgfx::TextureHandle getViewportTexture(ViewportHandle handle) override;
    int getBackbufferWidth() const override  { return m_width; }
    int getBackbufferHeight() const override { return m_height; }

    // -- Stretch Blit / Affine Blit (transform.lua GPU path) ----------
    void stretchBlt(uint16_t targetView, uint32_t dstTexId,
                     float dx, float dy, float dw, float dh,
                     uint32_t srcTexId,
                     float sx, float sy, float sw, float sh,
                     int filterType) override;

    void affineBlt(uint16_t targetView, uint32_t dstTexId,
                    float dx, float dy, float dw, float dh,
                    uint32_t srcTexId,
                    float sx, float sy, float sw, float sh,
                    const float matrix[6]) override;

    void setDebugName(uint16_t viewId, const std::string& name) override;
    void renderText(uint16_t viewId, const std::string& text,
                    float x, float y,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a) override;
    void renderRuby(uint16_t viewId, const std::string& text,
                    const std::string& ruby,
                    float x, float y,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a) override;
    void setFont(int fontId) override;
    float textLineHeight() const override;
    void flushAllRTT() override;

    bgfx::ProgramHandle getFallbackProgram() const override { return m_fallbackProgram; }
    const bgfx::VertexLayout& getPosTexLayout() const { return m_posTexLayout; }
    bgfx::UniformHandle getDefaultSampler() const override { return m_texSampler; }

    bgfx::ProgramHandle getBlendProgram()      const { return m_blendProgram; }
    bgfx::ProgramHandle getTransitionProgram() const { return m_transitionProgram; }
    bgfx::ProgramHandle getVFXProgram()        const { return m_vfxProgram; }
    bgfx::UniformHandle getBlendParams()       const { return m_u_blendParams; }
    bgfx::UniformHandle getTransParams()       const { return m_u_transParams; }
    bgfx::UniformHandle getVFXParams()         const { return m_u_vfxParams; }

    // Stretch/Affine shader accessors
    bgfx::ProgramHandle getStretchProgram()    const { return m_stretchProgram; }
    bgfx::ProgramHandle getAffineProgram()     const { return m_affineProgram; }
    bgfx::UniformHandle getStretchParams()     const { return m_u_stretchParams; }
    bgfx::UniformHandle getAffineParams()      const { return m_u_affineParams; }

    void submitBlend(uint16_t viewId, bgfx::TextureHandle baseTex,
                     bgfx::TextureHandle blendTex, int mode,
                     float baseAlpha, float blendAlpha, float globalAlpha);
    void submitTransition(uint16_t viewId, bgfx::TextureHandle fromTex,
                          bgfx::TextureHandle toTex, bgfx::TextureHandle ruleTex,
                          int method, float progress);
    void submitVFX(uint16_t viewId, bgfx::TextureHandle srcTex,
                   int effect, float fadeAlpha, float fadeR, float fadeG, float fadeB,
                   float blurRadius, float quakeX, float quakeY);
    void fillViewport(ViewportHandle handle, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    // -- Batch protocol (spec [0.3])
    void beginBatch() override;
    void flushBatch() override;


private:
    void initEmbeddedShaders();
    void setupDefaultViews();

    int m_width  = 1280;
    int m_height = 720;
    bool m_bgfxInitialized = false;

    bgfx::ProgramHandle m_fallbackProgram = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout   m_posTexLayout;
    bgfx::UniformHandle  m_texSampler = BGFX_INVALID_HANDLE;

    bgfx::ProgramHandle m_blendProgram      = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_transitionProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_vfxProgram        = BGFX_INVALID_HANDLE;

    // Stretch / Affine programs and uniforms
    bgfx::ProgramHandle m_stretchProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_affineProgram  = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle m_u_blendParams   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_transParams   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_vfxParams     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_stretchParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_affineParams  = BGFX_INVALID_HANDLE;

    struct RTTEntry {
        bgfx::FrameBufferHandle fb    = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle     tex   = BGFX_INVALID_HANDLE;
        uint16_t                viewId = VIEW_RTT;
    };

    uint32_t m_nextHandle = 1;
    std::unordered_map<uint32_t, RTTEntry> m_rttMap;
    std::unique_ptr<TextRenderer> m_textRenderer;

    // Batch protocol (spec [0.3])
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

// -- Shutdown coordination: signal bgfx debug callback before GPU teardown --
void setBgfxShuttingDown(bool shuttingDown);
