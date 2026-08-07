 #include "BgfxRenderDevice.h"
#include "BgfxDebugCallback.h"
#include "ShaderCache.h"
#include "../di/api/ThreadAssert.h"
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



namespace Caesura {

namespace {

RenderTextureHandle toRenderHandle(bgfx::TextureHandle handle) {
    return bgfx::isValid(handle) ? RenderTextureHandle{handle.idx} : RenderTextureHandle{};
}

RenderProgramHandle toRenderHandle(bgfx::ProgramHandle handle) {
    return bgfx::isValid(handle) ? RenderProgramHandle{handle.idx} : RenderProgramHandle{};
}

RenderUniformHandle toRenderHandle(bgfx::UniformHandle handle) {
    return bgfx::isValid(handle) ? RenderUniformHandle{handle.idx} : RenderUniformHandle{};
}

bgfx::TextureHandle toBgfx(RenderTextureHandle handle) {
    if (!handle.isValid()) return BGFX_INVALID_HANDLE;
    bgfx::TextureHandle result;
    result.idx = handle.idx;
    return result;
}

} // namespace

BgfxRenderDevice::~BgfxRenderDevice() {
    shutdown();
}



void BgfxRenderDevice::flushAllRTT() {
    if (m_bgfxInitialized && m_deviceCore) m_deviceCore->flushAllRTT();
}


// ===========================================================================
//  Batch protocol (spec [0.3]): beginBatch / flushBatch
// ===========================================================================

// ===========================================================================
//  Batch protocol (spec [0.3]): beginBatch / flushBatch
//  Defers GPU submission to batch many draw calls into one vertex buffer.
// ===========================================================================


void BgfxRenderDevice::beginBatch() { m_draw->beginBatch(); }


void BgfxRenderDevice::flushBatch() { m_draw->flushBatch(); }


// ===========================================================================
// Backend preference helpers
// These correspond to the original objective's requirement for explicit
// DX12 / Metal / WebGPU backend stubs. bgfx handles the actual backend
// internally; these helpers expose the selection API.

static bgfx::RendererType::Enum s_preferredBackend = bgfx::RendererType::Direct3D11;

// setPreferredBackend extracted to BgfxDeviceCore


// getBackendName extracted to BgfxDeviceCore


bool BgfxRenderDevice::init(void* nativeWindowHandle, int width, int height) {
    m_bgfxInitialized = false;
    m_shutdownComplete = false;
    m_shaders = std::make_unique<BgfxShaderManager>();
    m_deviceCore = std::make_unique<BgfxDeviceCore>();
    if (!m_deviceCore->init(nativeWindowHandle, width, height)) {
        m_deviceCore.reset();
        m_shaders.reset();
        return false;
    }
    m_bgfxInitialized = true;
    m_shaders->initEmbeddedShaders();
    m_drawState.shaders = m_shaders.get();
    m_drawState.device  = m_deviceCore.get();
    m_draw = std::make_unique<BgfxDraw>();
    m_draw->init(&m_drawState);
    m_textRenderer = std::make_unique<TextRenderer>();
    if (!m_textRenderer->init(this)) { m_textRenderer.reset(); }
    return true;
}

void BgfxRenderDevice::beginShutdown() {
    if (m_bgfxInitialized) setBgfxShuttingDown(true);
}

void BgfxRenderDevice::resize(int width, int height) { m_deviceCore->resize(width, height); }


void BgfxRenderDevice::shutdown() {
    if (m_shutdownComplete) return;
    m_shutdownComplete = true;
    if (m_textRenderer) m_textRenderer.reset();
    m_shaders.reset();
    if (m_deviceCore) m_deviceCore->shutdown();
    m_bgfxInitialized = false;
}




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




// initEmbeddedShaders
// Picks the correct embedded bytecode (SPIR-V for Vulkan, DXBC for
// D3D11/D3D12), wraps it in a proper bgfx binary header via
// buildBgfxShader(), and registers the resulting program as the
// engine-wide fallback for 2-D quad rendering and RTT blits.




//setupDefaultViews      configure the three View layers
//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T

// setupDefaultViews extracted to BgfxDeviceCore



//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T
// Frame-management pass-throughs
//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T

void BgfxRenderDevice::beginFrame() {
    if (m_bgfxInitialized && m_deviceCore) m_deviceCore->beginFrame();
}


void BgfxRenderDevice::endFrame() {
    if (m_bgfxInitialized && m_deviceCore) m_deviceCore->endFrame();
}


void BgfxRenderDevice::commit_frame() {
    if (m_bgfxInitialized && m_deviceCore) m_deviceCore->commit_frame();
}


void BgfxRenderDevice::advanceFrame() {
    if (m_bgfxInitialized && m_deviceCore) m_deviceCore->commit_frame();
}



//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T
// View-management pass-throughs
//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T

void BgfxRenderDevice::setScreenOffset(int dx, int dy) {
    if (m_deviceCore) m_deviceCore->setScreenOffset(dx, dy);
}

void BgfxRenderDevice::setViewRect(uint16_t v, uint16_t x, uint16_t y, uint16_t w, uint16_t h) { m_deviceCore->setViewRect(v, x, y, w, h); }


void BgfxRenderDevice::setViewClear(uint16_t v, uint16_t f, uint32_t c, float d, uint8_t s) { m_deviceCore->setViewClear(v, f, c, d, s); }


void BgfxRenderDevice::touch(uint16_t v) { m_deviceCore->touch(v); }



//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T
// createRenderTarget / destroyRenderTarget / blitViewport
//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T

ViewportHandle BgfxRenderDevice::createRenderTarget(int w, int h) { return m_deviceCore->createRenderTarget(w, h); }


void BgfxRenderDevice::destroyRenderTarget(ViewportHandle h) { m_deviceCore->destroyRenderTarget(h); }


void BgfxRenderDevice::blitViewport(ViewportHandle handle, uint16_t targetView,
                                     float x, float y, float w, float h) {
    blitTexture(targetView, m_deviceCore->getViewportTexture(handle), x, y, w, h, 255);
}

RenderTextureHandle BgfxRenderDevice::getViewportTexture(ViewportHandle h) { return toRenderHandle(m_deviceCore->getViewportTexture(h)); }

RenderProgramHandle BgfxRenderDevice::getFallbackProgram() const { return toRenderHandle(m_shaders->getFallbackProgram()); }

RenderUniformHandle BgfxRenderDevice::getDefaultSampler() const { return toRenderHandle(m_shaders->getDefaultSampler()); }




void BgfxRenderDevice::blitTexture(uint16_t v, uint32_t tid, float x, float y, float w, float h, uint8_t o) { m_draw->blitTexture(v,tid,x,y,w,h,o); }
void BgfxRenderDevice::blitTexture(uint16_t v, bgfx::TextureHandle t, float x, float y, float w, float h, uint8_t o) { m_draw->blitTexture(v,t,x,y,w,h,o); }



// blitTexture(handle) old body removed



void BgfxRenderDevice::renderText(uint16_t viewId, const std::string& text,
                                     float x, float y,
                                     uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // Cached path: static text (same text/view/position every frame) reuses
    // its glyph geometry with zero rebuild; the full key guarantees a cache
    // hit is only ever served for identical parameters (see matches()).
    if (m_textRenderer)
        m_textRenderer->renderTextCached(viewId, text, x, y, TextColor{r,g,b,a});
}

void BgfxRenderDevice::renderRuby(uint16_t viewId, const std::string& text,
                                     const std::string& ruby,
                                     float x, float y,
                                     uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (m_textRenderer)
        m_textRenderer->renderRuby(viewId, text, ruby, x, y, TextColor{r,g,b,a});
}

void BgfxRenderDevice::setFont(int fontId) {
    if (m_textRenderer)
        m_textRenderer->setFont(static_cast<FontId>(fontId));
}

bool BgfxRenderDevice::loadTTF(const char* path, float fontSize) {
    return m_textRenderer && m_textRenderer->loadTTF(path, fontSize);
}

float BgfxRenderDevice::textLineHeight() const {
    return m_textRenderer ? m_textRenderer->lineHeight() : 16.0f;
}

void BgfxRenderDevice::setDebugName(uint16_t v, const std::string& n) { m_deviceCore->setDebugName(v, n.c_str()); }

void BgfxRenderDevice::drawDebugOverlay(const std::string& title) {
    const bgfx::Caps* caps = bgfx::getCaps();
    if (!caps) return;

    bgfx::dbgTextClear();
    bgfx::dbgTextPrintf(0, 0, 0x0F, "%s", title.c_str());
    bgfx::dbgTextPrintf(0, 1, 0x0F, "Renderer: %s  %dx%d",
                        bgfx::getRendererName(caps->rendererType),
                        getBackbufferWidth(), getBackbufferHeight());
}

bool BgfxRenderDevice::requestScreenshot(const std::string& path) {
    if (path.empty()) return false;
    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
    return true;
}

bool BgfxRenderDevice::recoverDevice(void* nativeWindowHandle, int width, int height) {
    if (!m_bgfxInitialized || !m_deviceCore) return false;

    if (m_textRenderer) {
        m_textRenderer->shutdown();
        m_textRenderer.reset();
    }
    m_draw.reset();
    m_shaders.reset();
    m_bgfxInitialized = false;
    m_deviceCore->shutdown();
    setBgfxShuttingDown(false);

    if (!m_deviceCore->init(nativeWindowHandle, width, height)) {
        return false;
    }
    m_bgfxInitialized = true;

    m_shaders = std::make_unique<BgfxShaderManager>();
    m_shaders->initEmbeddedShaders();
    m_drawState.shaders = m_shaders.get();
    m_drawState.device  = m_deviceCore.get();
    m_draw = std::make_unique<BgfxDraw>();
    m_draw->init(&m_drawState);
    m_textRenderer = std::make_unique<TextRenderer>();
    if (!m_textRenderer->init(this)) {
        m_textRenderer.reset();
    }
    return true;
}

void BgfxRenderDevice::flagDeviceLost() { BgfxDebugCallback::flagDeviceLost(); }

bool BgfxRenderDevice::consumeDeviceLost() { return BgfxDebugCallback::isDeviceLost(); }

RenderRuntimeInfo BgfxRenderDevice::getRuntimeInfo() const {
    RenderRuntimeInfo info;
    info.backendName = getBackendName();
    info.width = getBackbufferWidth();
    info.height = getBackbufferHeight();
    info.viewCount = 3;
    info.shaderReady = m_shaders && bgfx::isValid(m_shaders->getFallbackProgram());
    return info;
}



// ===========================================================================
//  GPU Effect: Blend -- two-texture blend with selectable mode

// ===========================================================================
//  fillViewport -- render solid-color quad into a viewport RTT framebuffer
// ===========================================================================

void BgfxRenderDevice::fillViewport(ViewportHandle h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) { m_draw->fillViewport(h,r,g,b,a); }

// Accessibility color filter presets (Machado et al. 2009 matrices):
// row-major 3x3, applied by effect-4 VFX passes (see BgfxDraw_Effects).
bool BgfxRenderDevice::setColorFilter(ColorFilterPreset preset) {
    if (!m_deviceCore) return false;
    static const float kDeuteranopia[9] = {
        0.367f, 0.861f, -0.228f,
        0.280f, 0.673f,  0.047f,
        -0.012f, 0.043f,  0.969f,
    };
    static const float kProtanopia[9] = {
        0.152f, 1.053f, -0.205f,
        0.115f, 0.786f,  0.099f,
        -0.004f, 0.028f,  0.976f,
    };
    static const float kTritanopia[9] = {
        1.256f, -0.077f, -0.179f,
        -0.078f, 0.931f,  0.148f,
        0.005f,  0.691f,  0.304f,
    };
    static const float kGrayscale[9] = {
        0.299f, 0.587f, 0.114f,
        0.299f, 0.587f, 0.114f,
        0.299f, 0.587f, 0.114f,
    };
    static const float kHighContrast[9] = {
        1.25f, 0.0f,  0.0f,
        0.0f,  1.25f, 0.0f,
        0.0f,  0.0f,  1.25f,
    };
    switch (preset) {
    case ColorFilterPreset::None:         m_deviceCore->setColorFilterMatrix(nullptr); break;
    case ColorFilterPreset::Deuteranopia: m_deviceCore->setColorFilterMatrix(kDeuteranopia); break;
    case ColorFilterPreset::Protanopia:   m_deviceCore->setColorFilterMatrix(kProtanopia); break;
    case ColorFilterPreset::Tritanopia:   m_deviceCore->setColorFilterMatrix(kTritanopia); break;
    case ColorFilterPreset::Grayscale:    m_deviceCore->setColorFilterMatrix(kGrayscale); break;
    case ColorFilterPreset::HighContrast: m_deviceCore->setColorFilterMatrix(kHighContrast); break;
    default: return false;
    }
    return true;
}


// ===========================================================================


// ===========================================================================
//  submitFullscreenQuad  helper for GPU effects
// ===========================================================================

// x,y reserved for future 3D RTT offset rendering
// submitFullscreenQuad → BgfxDraw


void BgfxRenderDevice::submitBlend(uint16_t v, RenderTextureHandle base, RenderTextureHandle blend, int mode, float ba, float bla, float ga) { m_draw->submitBlend(v,toBgfx(base),toBgfx(blend),mode,ba,bla,ga); }


// ===========================================================================
//  GPU Effect: Transition ?? crossfade / rule / wipe between two textures
// ===========================================================================

// Spec [10.2.25]: @Beta 闂?Pre-bake rule images into a LUT texture atlas for batch
// transition rendering. Currently each transition passes its rule texture
// individually via texture slot 2. A pre-baked atlas would reduce draw calls.
void BgfxRenderDevice::submitTransition(uint16_t v, RenderTextureHandle from, RenderTextureHandle to, RenderTextureHandle rule, int method, float progress) { m_draw->submitTransition(v,toBgfx(from),toBgfx(to),toBgfx(rule),method,progress); }


// ===========================================================================
//  GPU Effect: VFX ?? fade / blur / quake post-processing
// ===========================================================================

void BgfxRenderDevice::submitVFX(uint16_t v, RenderTextureHandle src, int e, float fa, float fr, float fg, float fb, float br, float qx, float qy) { m_draw->submitVFX(v,toBgfx(src),e,fa,fr,fg,fb,br,qx,qy); }


// ===========================================================================
//  GPU Transform: Stretch Blit (filtered copy with src/dst rects)
// ===========================================================================

void BgfxRenderDevice::stretchBlt(uint16_t v, uint32_t d, float dx, float dy, float dw, float dh, uint32_t s, float sx, float sy, float sw, float sh, int f) { m_draw->stretchBlt(v,d,dx,dy,dw,dh,s,sx,sy,sw,sh,f); }


// ===========================================================================
//  GPU Transform: Affine Blit (2D affine matrix transform)
// ===========================================================================

void BgfxRenderDevice::affineBlt(uint16_t v, uint32_t d, float dx, float dy, float dw, float dh, uint32_t s, float sx, float sy, float sw, float sh, const float m[6]) { m_draw->affineBlt(v,d,dx,dy,dw,dh,s,sx,sy,sw,sh,m); }


} // namespace Caesura
