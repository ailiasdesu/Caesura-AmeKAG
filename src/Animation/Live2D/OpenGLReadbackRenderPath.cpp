#ifdef CAESURA_HAS_LIVE2D

#include "OpenGLReadbackRenderPath.h"

// GL MUST come before Cubism headers
#ifdef _WIN32
#include <GL/glew.h>
#endif

#include <Rendering/CubismRenderer.hpp>
#include <Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp>

#include <SDL3/SDL.h>
#include <cstring>

namespace Caesura {

using namespace Csm;
using namespace Csm::Rendering;

bool OpenGLReadbackRenderPath::init(int width, int height) {
    m_width = width;
    m_height = height;
    m_pixelBuffer.resize(width * height * 4);

#ifdef _WIN32
    if (!m_glInitialized) {
        GLenum glewErr = glewInit();
        if (glewErr != GLEW_OK) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "[Live2D/GL] GLEW init warning: %s", glewGetErrorString(glewErr));
        }
    }
#endif
    m_glInitialized = true;
    SDL_Log("[Live2D/GL] Render path ready (%dx%d)", width, height);
    return true;
}

void OpenGLReadbackRenderPath::shutdown() {
    m_glInitialized = false;
    m_pixelBuffer.clear();
}

CubismRenderer* OpenGLReadbackRenderPath::createRenderer() {
    // CubismUserModel::CreateRenderer internally creates a CubismRenderer_OpenGLES2
    // This is called from Live2DBackend after model setup
    return nullptr;  // renderer is created by CubismUserModel, not by us
}

void OpenGLReadbackRenderPath::beginFrame(CubismRenderer* renderer) {
    (void)renderer;
    auto* glRenderer = static_cast<CubismRenderer_OpenGLES2*>(renderer);
    if (glRenderer) glRenderer->DrawModel();
}

void OpenGLReadbackRenderPath::endFrame(CubismRenderer* renderer, bgfx::TextureHandle bgfxTex) {
    (void)renderer;
    if (!bgfx::isValid(bgfxTex)) return;

    // glReadPixels from current framebuffer (Cubism FBO)
    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, m_pixelBuffer.data());

    // GL origin bottom-left → bgfx origin top-left
    std::vector<uint8_t> flipped(m_pixelBuffer.size());
    int rowSize = m_width * 4;
    for (int y = 0; y < m_height; ++y) {
        std::memcpy(flipped.data() + y * rowSize,
                    m_pixelBuffer.data() + (m_height - 1 - y) * rowSize,
                    rowSize);
    }

    auto* mem = bgfx::copy(flipped.data(), static_cast<uint32_t>(flipped.size()));
    bgfx::updateTexture2D(bgfxTex, 0, 0, 0, 0, m_width, m_height, mem);
}

void OpenGLReadbackRenderPath::resize(int width, int height) {
    m_width = width;
    m_height = height;
    m_pixelBuffer.resize(width * height * 4);
}

} // namespace Caesura

#endif