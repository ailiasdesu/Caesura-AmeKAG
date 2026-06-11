#include "BgfxDeviceCore.h"
#include "BgfxShaderManager.h"
#include "BgfxDebugCallback.h"
#include <bx/math.h>
#include <cstdio>

namespace Caesura {

        if (!bgfx::isValid(vs) || !bgfx::isValid(fs)) {
            printf("[BgfxRenderDevice] %s: shader build failed.\n", name);
            if (bgfx::isValid(vs)) bgfx::destroy(vs);
            if (bgfx::isValid(fs)) bgfx::destroy(fs);
            return BGFX_INVALID_HANDLE;
        }
        bgfx::ProgramHandle prog = bgfx::createProgram(vs, fs, true);
        printf("[BgfxRenderDevice] %s program %s.\n", name,
               bgfx::isValid(prog) ? "READY" : "FAILED");
        return prog;
    };

    if (renderer == bgfx::RendererType::Direct3D11 ||
        renderer == bgfx::RendererType::Direct3D12) {

        m_shaders->getBlendProgram() = createProgramFromDXBC(
            kEmbeddedDXBC_vs_fullscreen, kEmbeddedDXBC_vs_fullscreen_size,
            kEmbeddedDXBC_fs_blend, kEmbeddedDXBC_fs_blend_size, "Blend");

        m_shaders->getTransitionProgram() = createProgramFromDXBC(
            kEmbeddedDXBC_vs_fullscreen, kEmbeddedDXBC_vs_fullscreen_size,
            kEmbeddedDXBC_fs_transition, kEmbeddedDXBC_fs_transition_size, "Transition");

        m_shaders->getVFXProgram() = createProgramFromDXBC(
            kEmbeddedDXBC_vs_fullscreen, kEmbeddedDXBC_vs_fullscreen_size,
            kEmbeddedDXBC_fs_vfx, kEmbeddedDXBC_fs_vfx_size, "VFX");
    }
        // Verify fallback program is valid before registering
    if (!bgfx::isValid(m_shaders->getFallbackProgram())) {
        fprintf(stderr, "[BgfxRenderDevice] FALLBACK PROGRAM INVALID, all rendering disabled!\n");
    }

    m_shaders->getStretchProgram() = createProgramFromDXBC(
            kEmbeddedDXBC_stretch_blt_vs, kEmbeddedDXBC_stretch_blt_vs_size,
            kEmbeddedDXBC_stretch_blt_fs, kEmbeddedDXBC_stretch_blt_fs_size, "StretchBlt");

        m_shaders->getAffineProgram() = createProgramFromDXBC(
            kEmbeddedDXBC_affine_blt_vs, kEmbeddedDXBC_affine_blt_vs_size,
            kEmbeddedDXBC_affine_blt_fs, kEmbeddedDXBC_affine_blt_fs_size, "AffineBlt");

    // -- Create uniform handles for effect cbuffers -------------------
    m_shaders->getBlendParams() = bgfx::createUniform("BlendParams",  bgfx::UniformType::Vec4, 2);
    m_shaders->getTransParams() = bgfx::createUniform("TransParams",  bgfx::UniformType::Vec4, 1);
    m_shaders->getVFXParams()   = bgfx::createUniform("VFXParams",    bgfx::UniformType::Vec4, 3);

    // -- Stretch / Affine uniforms ----------------------------------
    m_shaders->getStretchParams() = bgfx::createUniform("StretchParams", bgfx::UniformType::Vec4, 1);
    m_shaders->getAffineParams()  = bgfx::createUniform("AffineParams",  bgfx::UniformType::Vec4, 4);

    // -- Register with ShaderCache -------------------------------------
    if (bgfx::isValid(m_shaders->getBlendProgram())) {
        static const int kAlphaModes[] = {
            (int)BlendMode::Normal,    (int)BlendMode::Multiply,
            (int)BlendMode::Screen,    (int)BlendMode::Overlay,
            (int)BlendMode::Darken,    (int)BlendMode::Lighten,
            (int)BlendMode::Add,       (int)BlendMode::Difference,
            (int)BlendMode::Exclusion, (int)BlendMode::SoftLight
        };
        for (auto mode : kAlphaModes) {
            CompositeShaderKey key;
            key.blendMode  = mode;
            key.usePalette = false;
            CompositeShaderCache::instance().registerProgram(key, m_shaders->getBlendProgram());
        }
        printf("[BgfxRenderDevice] Registered 10 blend modes with ShaderCache.\n");
    CompositeShaderCache::instance().precompileCommon();
    }
    if (bgfx::isValid(m_shaders->getFallbackProgram())) {
        CompositeShaderKey fk;
        fk.blendMode  = static_cast<int>(BlendMode::Normal);
        fk.usePalette = false;
        CompositeShaderCache::instance().registerProgram(fk, m_shaders->getFallbackProgram());
    }
    // stretch/affine are singletons -- no ShaderCache registration needed.
}}


//setupDefaultViews      configure the three View layers
//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T

void BgfxDeviceCore::setupDefaultViews(BgfxShaderManager* shaders) {
    // -- View RTT (offscreen render target) --
    bgfx::setViewRect(VIEW_RTT, 0, 0, uint16_t(m_deviceCore->getWidth()), uint16_t(m_deviceCore->getHeight()));
    bgfx::setViewClear(VIEW_RTT, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0x00000000, 1.0f, 0);

    // -- View MAIN (primary compositing) --
    bgfx::setViewRect(VIEW_MAIN, 0, 0, uint16_t(m_deviceCore->getWidth()), uint16_t(m_deviceCore->getHeight()));
    bgfx::setViewClear(VIEW_MAIN, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0x303030FF, 1.0f, 0);

    // Set ortho projection for MAIN view (pixel coords 0->w, 0->h)
    const bgfx::Caps* caps = bgfx::getCaps();
    float orthoMain[16];
    bx::mtxOrtho(orthoMain, 0.0f, float(m_deviceCore->getWidth()), float(m_deviceCore->getHeight()), 0.0f,
                 -1.0f, 1.0f, 0.0f,
                 caps ? caps->homogeneousDepth : false,
                 bx::Handedness::Left);
    bgfx::setViewTransform(VIEW_MAIN, nullptr, orthoMain);

    // Set ortho projection for RTT view (same pixel-coord space)
    float orthoRTT[16];
    bx::mtxOrtho(orthoRTT, 0.0f, float(m_deviceCore->getWidth()), float(m_deviceCore->getHeight()), 0.0f,
                 -1.0f, 1.0f, 0.0f,
                 caps ? caps->homogeneousDepth : false,
                 bx::Handedness::Left);
    bgfx::setViewTransform(VIEW_RTT, nullptr, orthoRTT);

    // -- View DEBUG (engine HUD overlay) --
    bgfx::setViewRect(VIEW_DEBUG, 0, 0, uint16_t(m_deviceCore->getWidth()), uint16_t(m_deviceCore->getHeight()));
    bgfx::setViewClear(VIEW_DEBUG, BGFX_CLEAR_NONE, 0x00000000, 1.0f, 0);
}


//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T
// Frame-management pass-throughs
//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T

void BgfxDeviceCore::beginFrame() { m_deviceCore->beginFrame(); }

void BgfxDeviceCore::endFrame() { m_deviceCore->endFrame(); }

void BgfxDeviceCore::commit_frame() { m_deviceCore->commit_frame(); }


//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T
// View-management pass-throughs
//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T

void BgfxDeviceCore::setViewRect(uint16_t viewId, uint16_t x, uint16_t y, uint16_t width, uint16_t height) { m_deviceCore->setViewRect(viewId, x, y, width, height); }

void BgfxDeviceCore::setViewClear(uint16_t viewId, uint16_t flags, uint32_t rgba, float depth, uint8_t stencil) { m_deviceCore->setViewClear(viewId, flags, rgba, depth, stencil); }

void BgfxDeviceCore::touch(uint16_t viewId) { m_deviceCore->touch(viewId); }


//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T
// createRenderTarget / destroyRenderTarget / blitViewport
//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T

ViewportHandle BgfxDeviceCore::createRenderTarget(int width, int height) { return m_deviceCore->createRenderTarget(width, height); }

void BgfxDeviceCore::destroyRenderTarget(ViewportHandle handle) { m_deviceCore->destroyRenderTarget(handle); }

}

    bgfx::setState(state);
    bgfx::setTexture(0, m_shaders->getDefaultSampler(), srcTex);

    bgfx::ProgramHandle prog = bgfx::isValid(m_shaders->getAffineProgram()) ? m_shaders->getAffineProgram() : m_shaders->getFallbackProgram();
    bgfx::submit(targetView, prog);
}

} // namespace Caesura

} // namespace Caesura

void BgfxDeviceCore::flushAllRTT() {
    for (auto& entry : m_rttMap) {
        if (bgfx::isValid(entry.second.fb)) {
            bgfx::destroy(entry.second.fb);
        }
    }
    m_rttMap.clear();
}