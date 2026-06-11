 #include "BgfxRenderDevice.h"
#include "BgfxDebugCallback.h"
#include "ShaderCache.h"
#include "../di/thread/ThreadAssert.h"
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

BgfxRenderDevice::~BgfxRenderDevice() {
    shutdown();
}



void BgfxRenderDevice::flushAllRTT() { m_deviceCore->flushAllRTT(); }


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
    m_shaders = std::make_unique<BgfxShaderManager>();
    m_shaders = std::make_unique<BgfxShaderManager>();
    m_deviceCore = std::make_unique<BgfxDeviceCore>();
    if (!m_deviceCore->init(nativeWindowHandle, width, height)) return false;
    m_posTexLayout
        .begin()
        .add(bgfx::Attrib::Position,  2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
    m_drawState.shaders = m_shaders.get();
    m_drawState.device  = m_deviceCore.get();
    m_draw = std::make_unique<BgfxDraw>();
    m_draw->init(&m_drawState);
    m_textRenderer = std::make_unique<TextRenderer>();
    if (!m_textRenderer->init(this)) { m_textRenderer.reset(); }
    return true;
}

void BgfxRenderDevice::resize(int width, int height) { m_deviceCore->resize(width, height); }


void BgfxRenderDevice::shutdown() { if (m_textRenderer) m_textRenderer.reset(); m_shaders.reset(); m_deviceCore->shutdown(); }




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

void BgfxRenderDevice::beginFrame() { m_deviceCore->beginFrame(); }


void BgfxRenderDevice::endFrame() { m_deviceCore->endFrame(); }


void BgfxRenderDevice::commit_frame() { m_deviceCore->commit_frame(); }



//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T
// View-management pass-throughs
//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T

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
    auto it = m_rttMap.find(handle.id);
    if (it == m_rttMap.end() || !bgfx::isValid(it->second.tex)) return;

    blitTexture(targetView, m_deviceCore->getViewportTexture(handle), x, y, w, h, 255);
}

bgfx::TextureHandle BgfxRenderDevice::getViewportTexture(ViewportHandle h) { return m_deviceCore->getViewportTexture(h); }




void BgfxRenderDevice::blitTexture(uint16_t v, uint32_t tid, float x, float y, float w, float h, uint8_t o) { m_draw->blitTexture(v,tid,x,y,w,h,o); }
void BgfxRenderDevice::blitTexture(uint16_t v, bgfx::TextureHandle t, float x, float y, float w, float h, uint8_t o) { m_draw->blitTexture(v,t,x,y,w,h,o); }



// blitTexture(handle) old body removed



void BgfxRenderDevice::renderText(uint16_t viewId, const std::string& text,
                                     float x, float y,
                                     uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (m_textRenderer)
        m_textRenderer->renderText(viewId, text, x, y, TextColor{r,g,b,a});
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

float BgfxRenderDevice::textLineHeight() const {
    return m_textRenderer ? m_textRenderer->lineHeight() : 16.0f;
}

void BgfxRenderDevice::setDebugName(uint16_t v, const std::string& n) { m_deviceCore->setDebugName(v, n.c_str()); }



// ===========================================================================
//  GPU Effect: Blend -- two-texture blend with selectable mode

// ===========================================================================
//  fillViewport -- render solid-color quad into a viewport RTT framebuffer
// ===========================================================================

void BgfxRenderDevice::fillViewport(ViewportHandle h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) { m_draw->fillViewport(h,r,g,b,a); }


// ===========================================================================


// ===========================================================================
//  submitFullscreenQuad  helper for GPU effects
// ===========================================================================

// x,y reserved for future 3D RTT offset rendering
// submitFullscreenQuad → BgfxDraw


void BgfxRenderDevice::submitBlend(uint16_t v, bgfx::TextureHandle base, bgfx::TextureHandle blend, int mode, float ba, float bla, float ga) { m_draw->submitBlend(v,base,blend,mode,ba,bla,ga); }


// ===========================================================================
//  GPU Effect: Transition ?? crossfade / rule / wipe between two textures
// ===========================================================================

// Spec [10.2.25]: @Beta 闂?Pre-bake rule images into a LUT texture atlas for batch
// transition rendering. Currently each transition passes its rule texture
// individually via texture slot 2. A pre-baked atlas would reduce draw calls.
void BgfxRenderDevice::submitTransition(uint16_t v, bgfx::TextureHandle from, bgfx::TextureHandle to, bgfx::TextureHandle rule, int method, float progress) { m_draw->submitTransition(v,from,to,rule,method,progress); }


// ===========================================================================
//  GPU Effect: VFX ?? fade / blur / quake post-processing
// ===========================================================================

void BgfxRenderDevice::submitVFX(uint16_t v, bgfx::TextureHandle src, int e, float fa, float fr, float fg, float fb, float br, float qx, float qy) { m_draw->submitVFX(v,src,e,fa,fr,fg,fb,br,qx,qy); }


// ===========================================================================
//  GPU Transform: Stretch Blit (filtered copy with src/dst rects)
// ===========================================================================

void BgfxRenderDevice::stretchBlt(uint16_t v, uint32_t d, float dx, float dy, float dw, float dh, uint32_t s, float sx, float sy, float sw, float sh, int f) { m_draw->stretchBlt(v,d,dx,dy,dw,dh,s,sx,sy,sw,sh,f); }


// ===========================================================================
//  GPU Transform: Affine Blit (2D affine matrix transform)
// ===========================================================================

void BgfxRenderDevice::affineBlt(uint16_t v, uint32_t d, float dx, float dy, float dw, float dh, uint32_t s, float sx, float sy, float sw, float sh, const float m[6]) { m_draw->affineBlt(v,d,dx,dy,dw,dh,s,sx,sy,sw,sh,m); }


} // namespace Caesura


