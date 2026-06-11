#pragma once
#ifdef CAESURA_HAS_LIVE2D

#include "ILive2DRenderPath.h"

namespace Caesura {

// Linux optimal: Cubism OpenGL → glBlitFramebuffer → shared GL texture → bgfx.
// No CPU readback. No GLEW. No separate GL context. Uses bgfx's existing GL context.
class OpenGLSharedRenderPath : public ILive2DRenderPath {
public:
    bool init(int width, int height) override;
    void shutdown() override;
    CsmRendering::CubismRenderer* createRenderer() override;
    void beginFrame(CsmRendering::CubismRenderer* renderer) override;
    void endFrame(CsmRendering::CubismRenderer* renderer, bgfx::TextureHandle bgfxTex) override;
    void resize(int width, int height) override;
    const char* name() const override { return "OpenGLShared"; }

private:
    bool createTargetTexture(int width, int height);

    unsigned m_glTex = 0;       // GL texture for bgfx sharing
    unsigned m_blitFBO = 0;     // FBO with m_glTex as color attachment
    unsigned m_readFBO = 0;     // Cubism's read FBO (captured at first frame)
    int m_width = 1280;
    int m_height = 720;
    bool m_capturedReadFBO = false;
};

} // namespace Caesura

#endif