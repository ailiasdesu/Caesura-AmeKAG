 #include "BgfxRenderDevice.h"
#include "BgfxDebugCallback.h"
#include "ShaderCache.h"
#include "BgfxShaderManager.h"
#include "Core/ThreadAssert.h"
#include <bgfx/bgfx.h>
// #include <bgfx/embedded_shader.h> -- using bgfx::createShader with raw bytecode instead
#include <bx/math.h>
#include <bgfx/platform.h>
#include <bimg/decode.h>
#include <bx/bx.h>
#include <bx/readerwriter.h>
#include <bx/error.h>
#include <cstdio>
#include <cstring>


// bgfx debug callback captures internal error/warning messages during init
// BgfxDebugCallback extracted to BgfxDebugCallback.h/.cpp

// -- Shutdown coordination: called by Engine::shutdown() before GPU teardown --


namespace Caesura {

BgfxRenderDevice::~BgfxRenderDevice() {
    shutdown();
}




// -- Draw delegation (extracted to BgfxDraw) -----------------------------
void BgfxRenderDevice::flushAllRTT()                                        { flushAllRTT(); }
void BgfxRenderDevice::beginBatch()                                         { beginBatch(); }
void BgfxRenderDevice::flushBatch()                                         { flushBatch(); }
void BgfxRenderDevice::blitViewport(ViewportHandle h, uint16_t v, float x, float y, float w, float h2)
    { blitViewport(h, v, x, y, w, h2); }
void BgfxRenderDevice::blitTexture(uint16_t v, uint32_t tid, float x, float y, float w, float h, uint8_t o)
    { blitTexture(v, tid, x, y, w, h, o); }
void BgfxRenderDevice::blitTexture(uint16_t v, bgfx::TextureHandle t, float x, float y, float w, float h, uint8_t o)
    { blitTexture(v, t, x, y, w, h, o); }
void BgfxRenderDevice::fillViewport(ViewportHandle h, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    { fillViewport(h, r, g, b, a); }
void BgfxRenderDevice::stretchBlt(uint16_t v, uint32_t d, float dx, float dy, float dw, float dh,
    uint32_t s, float sx, float sy, float sw, float sh, int f)
    { stretchBlt(v, d, dx, dy, dw, dh, s, sx, sy, sw, sh, f); }
void BgfxRenderDevice::affineBlt(uint16_t v, uint32_t d, float dx, float dy, float dw, float dh,
    uint32_t s, float sx, float sy, float sw, float sh, const float m[6])
    { affineBlt(v, d, dx, dy, dw, dh, s, sx, sy, sw, sh, m); }
void BgfxRenderDevice::submitBlend(uint16_t v, bgfx::TextureHandle base, bgfx::TextureHandle blend,
    int mode, float ba, float bla, float ga)
    { submitBlend(v, base, blend, mode, ba, bla, ga); }
void BgfxRenderDevice::submitTransition(uint16_t v, bgfx::TextureHandle from, bgfx::TextureHandle to,
    bgfx::TextureHandle rule, int method, float progress)
    { submitTransition(v, from, to, rule, method, progress); }
void BgfxRenderDevice::submitVFX(uint16_t v, bgfx::TextureHandle src, int effect,
    float fa, float fr, float fg, float fb, float br, float qx, float qy)
    { submitVFX(v, src, effect, fa, fr, fg, fb, br, qx, qy); }
// Backend preference helpers
// These correspond to the original objective's requirement for explicit
// DX12 / Metal / WebGPU backend stubs. bgfx handles the actual backend
// internally; these helpers expose the selection API.

static bgfx::RendererType::Enum s_preferredBackend = bgfx::RendererType::Direct3D11;

bool BgfxRenderDevice::setPreferredBackend(const char* name) {
    if (strcmp(name, "vulkan") == 0 || strcmp(name, "Vulkan") == 0) {
        s_preferredBackend = bgfx::RendererType::Vulkan;
    } else if (strcmp(name, "dx12") == 0 || strcmp(name, "DirectX12") == 0) {
        s_preferredBackend = bgfx::RendererType::Direct3D12;
    } else if (strcmp(name, "dx11") == 0 || strcmp(name, "DirectX11") == 0) {
        s_preferredBackend = bgfx::RendererType::Direct3D11;
    } else if (strcmp(name, "metal") == 0 || strcmp(name, "Metal") == 0) {
        s_preferredBackend = bgfx::RendererType::Metal;
    } else if (strcmp(name, "webgpu") == 0 || strcmp(name, "WebGPU") == 0) {
        s_preferredBackend = bgfx::RendererType::WebGPU;
    } else if (strcmp(name, "opengl") == 0 || strcmp(name, "OpenGL") == 0) {
        s_preferredBackend = bgfx::RendererType::OpenGL;
    } else {
        fprintf(stderr, "[BgfxRenderDevice] Unknown backend: %s\n", name);
        return false;
    }
    printf("[BgfxRenderDevice] Preferred backend set to: %s\n", name);
    return true;
}

const char* BgfxRenderDevice::getBackendName() const {
    return bgfx::getRendererName(bgfx::getCaps()->rendererType);
}

bool BgfxRenderDevice::init(void* nativeWindowHandle, int width, int height) {
    m_deviceCore = std::make_unique<BgfxDeviceCore>();
    if (!m_deviceCore->init(nativeWindowHandle, width, height, m_shaders.get())) return false;
    m_deviceCore->getWidth() = width; m_deviceCore->getHeight() = height;
    // m_posTexLayout moved to BgfxDraw::init()
    m_textRenderer = std::make_unique<TextRenderer>();
    if (!m_textRenderer->init(this)) { m_textRenderer.reset(); }
    m_bgfxInitialized = true;
    return true;
}

void BgfxRenderDevice::resize(int width, int height) { m_deviceCore->getWidth() = width; m_deviceCore->getHeight() = height; m_deviceCore->resize(width, height); }

void BgfxRenderDevice::shutdown() { if (m_textRenderer) m_textRenderer.reset(); m_shaders.reset(); m_deviceCore->shutdown(); m_bgfxInitialized = false; }



// Helper: construct a valid bgfx shader binary (version 11) from raw
// DXBC / SPIR-V bytecode. bgfx encodes its own header in front of the
// platform-specific code so the runtime can reflect on uniforms and
// input attributes without invoking the platform compiler.
//
// bgfx binary format (shader version >= 10):
//   uint32_t  magic          VSH/FSH/CSH + version byte (11)
//   uint32_t  hashIn
//   uint32_t  hashOut
//   uint16_t  uniformCount
//   ...       uniforms       (omitted when count == 0)
//   uint32_t  codeSize
//   uint8_t   code[codeSize]
//   uint8_t   padding        (1 byte)
//   uint8_t   numAttrs
//   uint16_t  attrIds[numAttrs]
//   uint16_t  cbSize         constant-buffer size, 0 when none

// buildBgfxShader moved to BgfxShaderManager.cpp


// initEmbeddedShaders
// Picks the correct embedded bytecode (SPIR-V for Vulkan, DXBC for
// D3D11/D3D12), wraps it in a proper bgfx binary header via
// buildBgfxShader(), and registers the resulting program as the
// engine-wide fallback for 2-D quad rendering and RTT blits.

void BgfxRenderDevice::initEmbeddedShaders() {
    const bgfx::RendererType::Enum renderer = bgfx::getCaps()->rendererType;

    printf("[BgfxRenderDevice] initEmbeddedShaders: renderer=%s\n",
           bgfx::getRendererName(renderer));
    // Initialize ShaderCache before registering any programs
    CompositeShaderCache::instance().init();

    const uint8_t*  vsCode     = nullptr;
    uint32_t        vsCodeSize = 0;
    const uint8_t*  fsCode     = nullptr;
    uint32_t        fsCodeSize = 0;

    // bgfx RendererType: 2=Direct3D11, 3=Direct3D12, 9=Vulkan
    if (renderer == bgfx::RendererType::Vulkan) {
        if (kEmbeddedVS_SpriteSize > 0 && kEmbeddedFS_TextureSize > 0) {
            vsCode     = reinterpret_cast<const uint8_t*>(kEmbeddedVS_Sprite);
            vsCodeSize = uint32_t(kEmbeddedVS_SpriteSize * sizeof(uint32_t));
            fsCode     = reinterpret_cast<const uint8_t*>(kEmbeddedFS_Texture);
            fsCodeSize = uint32_t(kEmbeddedFS_TextureSize * sizeof(uint32_t));
        }

    } else if (renderer == bgfx::RendererType::Direct3D11 ||
               renderer == bgfx::RendererType::Direct3D12) {
        if (kEmbeddedDXBC_VS_Sprite_size > 0 && kEmbeddedDXBC_FS_Texture_size > 0) {
            vsCode     = kEmbeddedDXBC_VS_Sprite;
            vsCodeSize = uint32_t(kEmbeddedDXBC_VS_Sprite_size);
            fsCode     = kEmbeddedDXBC_FS_Texture;
            fsCodeSize = uint32_t(kEmbeddedDXBC_FS_Texture_size);
        }
    }

    if (!vsCode || !fsCode) {
        printf("[BgfxRenderDevice] No embedded shaders for %s. "
               "Debug text only.\n", bgfx::getRendererName(renderer));
        return;
    }

    // Vertex shader input: Position (0x0001) + TexCoord0 (0x0010)
    const uint16_t vsAttrs[] = { 0x0001, 0x0010 };

    bgfx::ShaderHandle vs = buildBgfxShader(vsCode, vsCodeSize, false, 2, vsAttrs);
    bgfx::ShaderHandle fs = buildBgfxShader(fsCode, fsCodeSize, true,  0, nullptr);

    if (!bgfx::isValid(vs) || !bgfx::isValid(fs)) {
        fprintf(stderr, "[BgfxRenderDevice] buildBgfxShader failed.\n");
        if (bgfx::isValid(vs)) bgfx::destroy(vs);
        if (bgfx::isValid(fs)) bgfx::destroy(fs);
        return;
    }

    printf("[BgfxRenderDevice] Shader binaries built (VS=%u B, FS=%u B).\n",
           vsCodeSize, fsCodeSize);

    m_shaders->getFallbackProgram() = bgfx::createProgram(vs, fs, true);

    if (!bgfx::isValid(m_shaders->getFallbackProgram())) {
        fprintf(stderr, "[BgfxRenderDevice] createProgram rejected shaders. "
                "Debug text only.\n");
    } else {
        
    // -- Create effect shader programs from embedded DXBC -------------
    auto createProgramFromDXBC = [&](const uint8_t* vsData, size_t vsSize,
                                      const uint8_t* fsData, size_t fsSize,
                                      const char* name) -> bgfx::ProgramHandle {
        if (!vsData || !fsData || vsSize == 0 || fsSize == 0) {
            printf("[BgfxRenderDevice] %s: no embedded data.\n", name);
            return BGFX_INVALID_HANDLE;
        }
        const uint16_t vsAttrs[] = { 0x0001, 0x0010 };
        bgfx::ShaderHandle vs = buildBgfxShader(vsData, (uint32_t)vsSize, false, 2, vsAttrs);
        bgfx::ShaderHandle fs = buildBgfxShader(fsData, (uint32_t)fsSize, true, 0, nullptr);
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

void BgfxRenderDevice::setupDefaultViews() {
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

void BgfxRenderDevice::beginFrame() { m_deviceCore->beginFrame(); }

void BgfxRenderDevice::endFrame() { m_deviceCore->endFrame(); }

void BgfxRenderDevice::commit_frame() { m_deviceCore->commit_frame(); }


//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T
// View-management pass-throughs
//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T

void BgfxRenderDevice::setViewRect(uint16_t viewId, uint16_t x, uint16_t y, uint16_t width, uint16_t height) { m_deviceCore->setViewRect(viewId, x, y, width, height); }

void BgfxRenderDevice::setViewClear(uint16_t viewId, uint16_t flags, uint32_t rgba, float depth, uint8_t stencil) { m_deviceCore->setViewClear(viewId, flags, rgba, depth, stencil); }

void BgfxRenderDevice::touch(uint16_t viewId) { m_deviceCore->touch(viewId); }


//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T
// createRenderTarget / destroyRenderTarget / blitViewport
//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T

ViewportHandle BgfxRenderDevice::createRenderTarget(int width, int height) { return m_deviceCore->createRenderTarget(width, height); }

void BgfxRenderDevice::destroyRenderTarget(ViewportHandle handle) { m_deviceCore->destroyRenderTarget(handle); }

}

    bgfx::setState(state);
    bgfx::setTexture(0, m_shaders->getDefaultSampler(), srcTex);

    bgfx::ProgramHandle prog = bgfx::isValid(m_shaders->getAffineProgram()) ? m_shaders->getAffineProgram() : m_shaders->getFallbackProgram();
    bgfx::submit(targetView, prog);
}

} // namespace Caesura

