#include "BgfxDeviceCore.h"
#include "BgfxShaderManager.h"
#include "TextRenderer.h"
#include <cstdio>

namespace Caesura {

// -- Preferred backend config ---------------------------------------------
static bgfx::RendererType::Enum s_preferredBackend = bgfx::RendererType::Direct3D11;

bool BgfxDeviceCore::init(void* nativeWindowHandle, int width, int height, BgfxShaderManager* shaders) {
    m_width  = width;
    m_height = height;

    bgfx::Init initParams;
    initParams.platformData.nwh = nativeWindowHandle;
    initParams.type     = s_preferredBackend;
    initParams.vendorId = BGFX_PCI_ID_NONE;
    initParams.resolution.width  = uint32_t(width);
    initParams.resolution.height = uint32_t(height);
    initParams.resolution.reset  = BGFX_RESET_VSYNC;
    initParams.profile  = false;
    // debug callback set by BgfxRenderDevice before calling init

    printf("[BgfxDeviceCore] nwh=%p, w=%d, h=%d, backend=%s\n",
           nativeWindowHandle, width, height, bgfx::getRendererName(s_preferredBackend));

    if (!bgfx::init(initParams)) {
        const char* preferredName = bgfx::getRendererName(s_preferredBackend);
        fprintf(stderr, "[BgfxDeviceCore] %s init failed; trying auto-select...\n", preferredName);
        bgfx::shutdown();
        initParams.type = bgfx::RendererType::Count;
        if (!bgfx::init(initParams)) {
            fprintf(stderr, "[BgfxDeviceCore] Fatal: bgfx::init failed.\n");
            return false;
        }
    }

    const bgfx::Caps* caps = bgfx::getCaps();
    printf("[BgfxDeviceCore] Renderer: %s (%s)\n",
           bgfx::getRendererName(caps->rendererType),
           caps->homogeneousDepth ? "homogeneous" : "non-homogeneous");

    bgfx::setDebug(BGFX_DEBUG_TEXT);
    setupDefaultViews();

    // Init shaders
    if (shaders) shaders->initEmbeddedShaders();

    bgfx::ViewId viewOrder[] = { VIEW_RTT, VIEW_MAIN, VIEW_DEBUG, VIEW_TRANSITION };
    bgfx::setViewOrder(0, 4, viewOrder);

    printf("[BgfxDeviceCore] Initialized %dx%d with 3 views\n", width, height);
    m_initialized = true;
    return true;
}

void BgfxDeviceCore::resize(int width, int height) {
    m_width  = width;
    m_height = height;
    bgfx::reset(uint32_t(width), uint32_t(height), BGFX_RESET_VSYNC);
    setupDefaultViews();
}

void BgfxDeviceCore::shutdown() {
    if (!m_initialized) return;
    for (auto& entry : m_rttMap) {
        if (bgfx::isValid(entry.second.fb)) bgfx::destroy(entry.second.fb);
    }
    m_rttMap.clear();
    m_initialized = false;
    bgfx::shutdown();
}

void BgfxDeviceCore::beginFrame() { /* bgfx::frame handled in endFrame */ }
void BgfxDeviceCore::endFrame()   { bgfx::frame(); }
void BgfxDeviceCore::commit_frame() { bgfx::frame(); }

void BgfxDeviceCore::setViewRect(uint16_t viewId, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    bgfx::setViewRect(viewId, x, y, w, h);
}
void BgfxDeviceCore::setViewClear(uint16_t viewId, uint16_t flags, uint32_t rgba, float depth, uint8_t stencil) {
    bgfx::setViewClear(viewId, flags, rgba, depth, stencil);
}
void BgfxDeviceCore::touch(uint16_t viewId) { bgfx::touch(viewId); }
void BgfxDeviceCore::setDebugName(uint16_t viewId, const char* name) { bgfx::setViewName(viewId, name); }

void BgfxDeviceCore::setupDefaultViews() {
    bgfx::setViewRect(VIEW_RTT, 0, 0, uint16_t(m_width), uint16_t(m_height));
    bgfx::setViewClear(VIEW_RTT, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x00000000, 1.0f, 0);

    bgfx::setViewRect(VIEW_MAIN, 0, 0, uint16_t(m_width), uint16_t(m_height));
    bgfx::setViewClear(VIEW_MAIN, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x00000000, 1.0f, 0);

    float ortho[16];
    bx::mtxOrtho(ortho, 0.0f, float(m_width), float(m_height), 0.0f, 0.0f, 1000.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(VIEW_MAIN, nullptr, ortho);
    bgfx::setViewTransform(VIEW_RTT, nullptr, ortho);

    bgfx::setViewRect(VIEW_DEBUG, 0, 0, uint16_t(m_width), uint16_t(m_height));
    bgfx::setViewClear(VIEW_DEBUG, BGFX_CLEAR_NONE, 0x00000000, 1.0f, 0);
}

uint32_t BgfxDeviceCore::createRenderTarget(int width, int height) {
    bgfx::TextureHandle tex = bgfx::createTexture2D(
        uint16_t(width), uint16_t(height), false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    if (!bgfx::isValid(tex)) return 0;

    bgfx::FrameBufferHandle fb = bgfx::createFrameBuffer(1, &tex, true);
    if (!bgfx::isValid(fb)) {
        bgfx::destroy(tex);
        return 0;
    }

    uint32_t handle = m_nextHandle++;
    m_rttMap[handle] = { fb, tex, VIEW_RTT };
    return handle;
}

void BgfxDeviceCore::destroyRenderTarget(uint32_t handle) {
    auto it = m_rttMap.find(handle);
    if (it == m_rttMap.end()) return;
    if (bgfx::isValid(it->second.fb)) bgfx::destroy(it->second.fb);
    m_rttMap.erase(it);
}

bgfx::TextureHandle BgfxDeviceCore::getViewportTexture(uint32_t handle) {
    auto it = m_rttMap.find(handle);
    return (it != m_rttMap.end()) ? it->second.tex : BGFX_INVALID_HANDLE;
}

} // namespace Caesura
