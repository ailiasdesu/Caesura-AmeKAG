#pragma once

#include "api/IRenderDevice.h"
#include <cstdint>  // fixed-width types (GCC strict)
#include "BgfxShaderManager.h"
#include "BgfxDeviceCore.h"
#include "BgfxDraw.h"
#include "TextRenderer.h"
#include <memory>
#include <bgfx/bgfx.h>
#include <string>

namespace Caesura {

class BgfxRenderDevice final : public IRenderDevice {
public:
    BgfxRenderDevice() = default;    ~BgfxRenderDevice() override;

    BgfxRenderDevice(const BgfxRenderDevice&) = delete;
    BgfxRenderDevice& operator=(const BgfxRenderDevice&) = delete;

    bool setPreferredBackend(const char* name) override { return BgfxDeviceCore::setPreferredBackend(name); }
    const char* getBackendName() const override { return m_deviceCore ? m_deviceCore->getBackendName() : "bgfx"; }

    bool init(void* nativeWindowHandle, int width, int height) override;
    bool isInitialized() const override { return m_bgfxInitialized; }
    void beginShutdown() override;
    void resize(int width, int height) override;
    void shutdown() override;
    void beginFrame() override;
    void endFrame() override;
    void commit_frame() override;
    void advanceFrame() override;
    void setScreenOffset(int dx, int dy) override;
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
    RenderTextureHandle getViewportTexture(ViewportHandle handle) override;
    int getBackbufferWidth() const override { return m_deviceCore ? m_deviceCore->getWidth() : m_width; }
    int getBackbufferHeight() const override { return m_deviceCore ? m_deviceCore->getHeight() : m_height; }

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
    void drawDebugOverlay(const std::string& title) override;
    bool requestScreenshot(const std::string& path) override;
    bool recoverDevice(void* nativeWindowHandle, int width, int height) override;
    void flagDeviceLost() override;
    bool consumeDeviceLost() override;
    RenderRuntimeInfo getRuntimeInfo() const override;
    void renderText(uint16_t viewId, const std::string& text,
                    float x, float y,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a) override;
    void renderRuby(uint16_t viewId, const std::string& text,
                    const std::string& ruby,
                    float x, float y,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a) override;
    void setFont(int fontId) override;
    bool loadTTF(const char* path, float fontSize) override;
    float textLineHeight() const override;
    void flushAllRTT() override;

    RenderProgramHandle getFallbackProgram() const override;
    RenderUniformHandle getDefaultSampler() const override;

    bgfx::ProgramHandle getBlendProgram()      const { return m_shaders->getBlendProgram(); }
    bgfx::ProgramHandle getTransitionProgram() const { return m_shaders->getTransitionProgram(); }
    bgfx::ProgramHandle getVFXProgram()        const { return m_shaders->getVFXProgram(); }
    bgfx::UniformHandle getBlendParams()       const { return m_shaders->getBlendParams(); }
    bgfx::UniformHandle getTransParams()       const { return m_shaders->getTransParams(); }
    bgfx::UniformHandle getVFXParams()         const { return m_shaders->getVFXParams(); }

    // Stretch/Affine shader accessors
    bgfx::ProgramHandle getStretchProgram()    const { return m_shaders->getStretchProgram(); }
    bgfx::ProgramHandle getAffineProgram()     const { return m_shaders->getAffineProgram(); }
    bgfx::UniformHandle getStretchParams()     const { return m_shaders->getStretchParams(); }
    bgfx::UniformHandle getAffineParams()      const { return m_shaders->getAffineParams(); }

    void submitBlend(uint16_t viewId, RenderTextureHandle baseTex, RenderTextureHandle blendTex, int mode, float baseAlpha, float blendAlpha, float globalAlpha) override;
    void submitTransition(uint16_t viewId, RenderTextureHandle fromTex, RenderTextureHandle toTex, RenderTextureHandle ruleTex, int method, float progress) override;
    void submitVFX(uint16_t viewId, RenderTextureHandle srcTex, int effect, float fadeAlpha, float fadeR, float fadeG, float fadeB, float blurRadius, float quakeX, float quakeY) override;
    void fillViewport(ViewportHandle handle, uint8_t r, uint8_t g, uint8_t b, uint8_t a) override;
    bool setColorFilter(ColorFilterPreset preset) override;

    // -- Batch protocol (spec [0.3])
    void beginBatch() override;
    void flushBatch() override;


private:
    // initEmbeddedShaders → BgfxShaderManager
    // setupDefaultViews → BgfxDeviceCore

    int m_width  = 1280;
    int m_height = 720;
    bool m_bgfxInitialized = false;

    std::unique_ptr<BgfxShaderManager> m_shaders;
    std::unique_ptr<BgfxDeviceCore>   m_deviceCore;
    DrawState m_drawState;
    bool m_shutdownComplete = false;
    std::unique_ptr<BgfxDraw>         m_draw;

    std::unique_ptr<TextRenderer> m_textRenderer;
};

} // namespace Caesura

// -- Shutdown coordination: signal bgfx debug callback before GPU teardown --
void setBgfxShuttingDown(bool shuttingDown);
