#pragma once
#ifdef CAESURA_HAS_LIVE2D

#include "ILive2DRenderPath.h"
#include <vector>
#include <cstdint>

#ifdef _WIN32
#include <GL/glew.h>
#elif defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

namespace Caesura {

// Cubism OpenGL ES 2 -> dedicated FBO -> glReadPixels -> bgfx updateTexture2D.
// Works everywhere OpenGL is available (Win/Linux/macOS).
// GPU->CPU->GPU per frame is viable for 2D at 60 fps.
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

    // Dedicated FBO for Cubism rendering (isolated from bgfx)
    unsigned int m_fbo = 0;
    unsigned int m_colorTex = 0;
    int m_savedFbo = 0;
    int m_savedViewport[4] = {0, 0, 0, 0};
};

} // namespace Caesura

#endif
