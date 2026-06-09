#ifdef CAESURA_HAS_LIVE2D

#include "OpenGLReadbackRenderPath.h"

// GL MUST come before Cubism headers
#ifdef _WIN32
#include <GL/glew.h>
#elif defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#include <Rendering/CubismRenderer.hpp>
#include <Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp>

#include <bgfx/bgfx.h>
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

    // Create FBO for Cubism rendering (isolate from bgfx's GL state)
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_colorTex);
    glBindTexture(GL_TEXTURE_2D, m_colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);

    GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fbStatus != GL_FRAMEBUFFER_COMPLETE) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[Live2D/GL] FBO incomplete: 0x%04X", fbStatus);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    SDL_Log("[Live2D/GL] Render path ready (%dx%d) with dedicated FBO", width, height);
    return true;
}

void OpenGLReadbackRenderPath::shutdown() {
    if (m_fbo)  { glDeleteFramebuffers(1, &m_fbo);  m_fbo = 0; }
    if (m_colorTex) { glDeleteTextures(1, &m_colorTex); m_colorTex = 0; }
    m_glInitialized = false;
    m_pixelBuffer.clear();
}

CubismRenderer* OpenGLReadbackRenderPath::createRenderer() {
    return nullptr;  // renderer is created by CubismUserModel, not by us
}

void OpenGLReadbackRenderPath::beginFrame(CubismRenderer* renderer) {
    if (!m_fbo || !m_colorTex) return;

    // Save bgfx's GL state before switching to Cubism FBO
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_savedFbo);
    glGetIntegerv(GL_VIEWPORT, m_savedViewport);

    // Bind our dedicated FBO for Cubism to render into
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    auto* glRenderer = static_cast<CubismRenderer_OpenGLES2*>(renderer);
    if (glRenderer) {
        glRenderer->DrawModel();
    }
}

void OpenGLReadbackRenderPath::endFrame(CubismRenderer* renderer, bgfx::TextureHandle bgfxTex) {
    (void)renderer;
    if (!bgfx::isValid(bgfxTex) || !m_fbo) {
        // Restore bgfx GL state even on failure
        if (m_savedFbo >= 0) glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)m_savedFbo);
        return;
    }

    // Read pixels from our FBO
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, m_pixelBuffer.data());

    // Restore bgfx's framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)m_savedFbo);
    glViewport(m_savedViewport[0], m_savedViewport[1], m_savedViewport[2], m_savedViewport[3]);

    // GL origin bottom-left -> bgfx origin top-left flip
    std::vector<uint8_t> flipped(m_pixelBuffer.size());
    int rowSize = m_width * 4;
    for (int y = 0; y < m_height; ++y) {
        std::memcpy(flipped.data() + y * rowSize,
                    m_pixelBuffer.data() + (m_height - 1 - y) * rowSize,
                    rowSize);
    }

    auto* mem = bgfx::copy(flipped.data(), static_cast<uint32_t>(flipped.size()));
    bgfx::updateTexture2D(bgfxTex, 0, 0, 0, 0,
                          static_cast<uint16_t>(m_width),
                          static_cast<uint16_t>(m_height), mem);
}

void OpenGLReadbackRenderPath::resize(int width, int height) {
    m_width = width;
    m_height = height;
    m_pixelBuffer.resize(width * height * 4);

    if (m_colorTex && m_fbo) {
        glBindTexture(GL_TEXTURE_2D, m_colorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

} // namespace Caesura

#endif
