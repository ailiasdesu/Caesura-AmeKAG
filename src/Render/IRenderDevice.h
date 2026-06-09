#pragma once
#include <cstdint>
#include <string>
#include <bgfx/bgfx.h>

namespace Caesura {

// -- View ID constants -----------------------------------------------------
// Render order enforced via bgfx::setViewOrder in IRenderDevice::init()
// VIEW_RTT (0) renders first   VIEW_MAIN (1) composites second   VIEW_DEBUG (2) last
constexpr uint16_t VIEW_RTT   = 0;  // Offscreen render-to-texture canvas
constexpr uint16_t VIEW_MAIN  = 1;  // Primary compositing pipeline (KAG UI)
constexpr uint16_t VIEW_DEBUG = 2;  // Debug overlay / IMGUI
constexpr uint16_t VIEW_TRANSITION = 99;  // Transition compositing view

// -- Handle types ----------------------------------------------------------

struct ViewportHandle {
    uint32_t id = 0;
    explicit operator bool() const { return id != 0; }
    bool operator==(const ViewportHandle& o) const { return id == o.id; }
    bool operator!=(const ViewportHandle& o) const { return id != o.id; }
};

// -- Abstract Render Device ------------------------------------------------

class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;

    // Lifecycle
    virtual bool init(void* nativeWindowHandle, int width, int height) = 0;

    // -- IMPORTANT: shutdown() internally calls flushAllRTT() then bgfx::shutdown() --
    // The teardown contract is:
    //   1. flushAllRTT()   release all GPU-side framebuffers and textures
    //      while the GPU context is still alive
    //   2. bgfx::shutdown()   destroy GPU context
    // Callers must ensure Lua VM is already dead before calling this.
    virtual void shutdown() = 0;

    // Explicitly release all RTT framebuffers/textures while GPU context is
    // still alive. Safe to call multiple times. Called automatically by shutdown().
    virtual void flushAllRTT() = 0;

    // Frame management
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void commit_frame() = 0;

    // View management
    virtual void setViewRect(uint16_t viewId, uint16_t x, uint16_t y,
                             uint16_t width, uint16_t height) = 0;
    virtual void setViewClear(uint16_t viewId, uint16_t flags,
                              uint32_t rgba, float depth, uint8_t stencil) = 0;
    virtual void touch(uint16_t viewId) = 0;

    // Offscreen render target
    virtual ViewportHandle createRenderTarget(int width, int height) = 0;
    virtual void destroyRenderTarget(ViewportHandle handle) = 0;

    // Draw a viewport`s texture as a full-view quad in another view
    virtual void blitViewport(ViewportHandle handle, uint16_t targetView,
                              float x, float y, float w, float h) = 0;

    // Get the bgfx texture from a viewport handle (rt->tex mapping for submit_batch)
    virtual bgfx::TextureHandle getViewportTexture(ViewportHandle handle) = 0;

    // Resolution query
    virtual int getBackbufferWidth() const = 0;
    virtual int getBackbufferHeight() const = 0;

    // Window resize -- must be called when the OS window changes size.
    // Recreates the backbuffer and re-applies all view transforms.
    virtual void resize(int width, int height) = 0;

    // Direct texture blit (submits a textured quad to the target view)
    // Used by RenderBinding::submit_batch to render layer quads.
    // The texture handle is an opaque integer (bgfx texture id).
    virtual void blitTexture(uint16_t targetView, uint32_t textureId,
                              float x, float y, float w, float h, uint8_t opacity) = 0;

    // -- Stretch Blit / Affine Blit (transform.lua GPU path) --------------
    // Stretch: blits src_rect -> dst_rect with sampler filter.
    // filterType: 0=Nearest, 1=Linear, 2=Anisotropic, 3+=Custom shader
    virtual void stretchBlt(uint16_t targetView, uint32_t dstTexId,
                             float dx, float dy, float dw, float dh,
                             uint32_t srcTexId,
                             float sx, float sy, float sw, float sh,
                             int filterType) = 0;

    // Affine: blits src_rect through 2D affine matrix onto dst rect.
    // matrix: {a, b, c, d, tx, ty} (2x3 row-major, column-vector convention:
    //   x` = a*x + c*y + tx
    //   y` = b*x + d*y + ty)
    virtual void affineBlt(uint16_t targetView, uint32_t dstTexId,
                            float dx, float dy, float dw, float dh,
                            uint32_t srcTexId,
                            float sx, float sy, float sw, float sh,
                            const float matrix[6]) = 0;

    // Debug marker
    
    // -- Batch protocol (performance optimization per spec [0.3]) ----------
    // beginBatch() defers GPU submission; flushBatch() submits all queued
    // draw calls at once, reducing draw-call overhead for multi-layer scenes.
    virtual void beginBatch() = 0;
    virtual void flushBatch() = 0;

    // Debug marker
    virtual void setDebugName(uint16_t viewId, const std::string& name) = 0;

    // -- Text rendering (bitmap font via embedded atlas) --------------
    virtual void renderText(uint16_t viewId, const std::string& text,
                             float x, float y,
                             uint8_t r, uint8_t g, uint8_t b, uint8_t a) = 0;
    virtual void renderRuby(uint16_t viewId, const std::string& text,
                             const std::string& ruby,
                             float x, float y,
                             uint8_t r, uint8_t g, uint8_t b, uint8_t a) = 0;
    virtual void setFont(int fontId) = 0;
    virtual float textLineHeight() const = 0;

    // -- Blend / Transition / VFX submission (P1: abstract interface methods) --
    virtual void submitBlend(uint16_t viewId, bgfx::TextureHandle baseTex,
                             bgfx::TextureHandle blendTex, int mode,
                             float baseAlpha, float blendAlpha, float globalAlpha) = 0;
    virtual void submitTransition(uint16_t viewId, bgfx::TextureHandle fromTex,
                                  bgfx::TextureHandle toTex, bgfx::TextureHandle ruleTex,
                                  int method, float progress) = 0;
    virtual void submitVFX(uint16_t viewId, bgfx::TextureHandle srcTex,
                           int effect, float fadeAlpha, float fadeR, float fadeG, float fadeB,
                           float blurRadius, float quakeX, float quakeY) = 0;
    virtual void fillViewport(ViewportHandle handle, uint8_t r, uint8_t g, uint8_t b, uint8_t a) = 0;

    // -- Shader / Sampler access (for ParticleSystem and other GPU systems) --
    virtual bgfx::UniformHandle getDefaultSampler() const { return BGFX_INVALID_HANDLE; }
    virtual bgfx::ProgramHandle getFallbackProgram() const { return BGFX_INVALID_HANDLE; }
};

} // namespace Caesura
