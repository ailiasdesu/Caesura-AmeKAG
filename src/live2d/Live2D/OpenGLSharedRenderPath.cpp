#ifdef CAESURA_LIVE2D

#include "OpenGLSharedRenderPath.h"

// GL MUST come before Cubism headers
#include <GL/glew.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

// bgfx
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

// Cubism
#include <Rendering/CubismRenderer.hpp>
#include <Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp>

#include <SDL3/SDL.h>
#include <cstring>
#include "debug/api/DebugLog.h"

namespace Caesura {

using namespace Csm;
using namespace Csm::Rendering;

// ============================================================
// init / shutdown
// ============================================================
bool OpenGLSharedRenderPath::init(int width, int height) {
    const GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
        DEBUG_WARN(SubSys::Live2D, ErrCode::Ok,
            "[Live2D/GL] GLEW init warning: %s", glewGetErrorString(glewErr));
    }
    m_width = width;
    m_height = height;
    return createTargetTexture(width, height);
}

void OpenGLSharedRenderPath::shutdown() {
    if (m_blitFBO) { glDeleteFramebuffers(1, &m_blitFBO); m_blitFBO = 0; }
    if (m_glTex)   { glDeleteTextures(1, &m_glTex);   m_glTex = 0;   }
    m_capturedReadFBO = false;
}

// ============================================================
// Target texture: GL texture → bgfx::overrideInternal
// ============================================================
bool OpenGLSharedRenderPath::createTargetTexture(int width, int height) {
    if (m_glTex) {
        glDeleteTextures(1, &m_glTex);
        glDeleteFramebuffers(1, &m_blitFBO);
    }

    // Create GL texture
    glGenTextures(1, &m_glTex);
    glBindTexture(GL_TEXTURE_2D, m_glTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Create FBO with this texture as color attachment
    glGenFramebuffers(1, &m_blitFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_blitFBO);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_glTex, 0);

    GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        DEBUG_ERR(SubSys::Live2D, ErrCode::Ok, "[Live2D/GL] FBO incomplete: 0x%04X", status);
        return false;
    }

    m_width = width;
    m_height = height;
    DEBUG_INFO(SubSys::Live2D, ErrCode::Ok, "[Live2D/GL] Shared GL texture + FBO ready (%dx%d)", width, height);
    return true;
}

// ============================================================
// Per-frame: Cubism → glBlitFramebuffer → shared texture → bgfx
// ============================================================
void OpenGLSharedRenderPath::beginFrame(CubismRenderer* renderer) {
    // Capture Cubism's read FBO on first frame (before DrawModel)
    if (!m_capturedReadFBO) {
        GLint prevRead = 0;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevRead);
        // Cubism hasn't drawn yet, so capture default framebuffer
        m_readFBO = (prevRead != 0) ? prevRead : 0;
    }

    auto* glRenderer = static_cast<CubismRenderer_OpenGLES2*>(renderer);
    if (glRenderer) {
        // Clear our target FBO to transparent
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_blitFBO);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        // Cubism renders to its own FBO
        glRenderer->DrawModel();

        // Read from Cubism's FBO
        GLint cubismFBO = 0;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &cubismFBO);
        if (cubismFBO > 0) {
            m_readFBO = cubismFBO;
            m_capturedReadFBO = true;
        }
    }
}

void OpenGLSharedRenderPath::endFrame(CubismRenderer* renderer, bgfx::TextureHandle bgfxTex) {
    (void)renderer;
    if (!m_glTex || !bgfx::isValid(bgfxTex)) return;

    // GPU-side blit: Cubism read FBO → our target FBO
    if (m_readFBO > 0) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_readFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_blitFBO);
        glBlitFramebuffer(0, 0, m_width, m_height,
                          0, 0, m_width, m_height,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }

    // Swap bgfx's internal texture with our GL texture (zero-copy from here)
    bgfx::overrideInternal(bgfxTex, static_cast<uintptr_t>(m_glTex));
}

void OpenGLSharedRenderPath::resize(int width, int height) {
    createTargetTexture(width, height);
}

} // namespace Caesura

#endif
