#pragma once
#ifdef CAESURA_HAS_LIVE2D

#include "ILive2DRenderPath.h"
#include <vector>
#include <cstdint>

namespace Caesura {

// Current default: Cubism OpenGL ES 2 FBO → glReadPixels → bgfx updateTexture2D.
// Works everywhere OpenGL is available, but GPU→CPU→GPU per frame is slow.
class OpenGLReadbackRenderPath : public ILive2DRenderPath {
public:
    bool init(int width, int height) override;
    void shutdown() override;
    CsmRendering::CubismRenderer* createRenderer() override;
    void beginFrame(CsmRendering::CubismRenderer* renderer) override;
    void endFrame(CsmRendering::CubismRenderer* renderer, bgfx::TextureHandle bgfxTex) override;
    void resize(int width, int height) override;
    const char* name() const override { return "OpenGLReadback"; }

private:
    int m_width = 1280;
    int m_height = 720;
    std::vector<uint8_t> m_pixelBuffer;
    bool m_glInitialized = false;
};

} // namespace Caesura

#endif