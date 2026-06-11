#include "BgfxDeviceCore.h"
#include "BgfxShaderManager.h"
#include "../di/thread/ThreadAssert.h"
#include "BgfxDebugCallback.h"
#include <bx/math.h>
#include <bx/bx.h>
#include <cstdio>

namespace Caesura {

static bgfx::RendererType::Enum s_preferredBackend = bgfx::RendererType::Direct3D11;

BgfxDeviceCore::~BgfxDeviceCore() { shutdown(); }

bool BgfxDeviceCore::setPreferredBackend(const char* name) {
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

const char* BgfxDeviceCore::getBackendName() const {
    return bgfx::getRendererName(bgfx::getCaps()->rendererType);
}

bool BgfxDeviceCore::init(void* nativeWindowHandle, int width, int height) {
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
    initParams.callback = &g_bgfxDebugCallback;

    if (!bgfx::init(initParams)) {
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



    bgfx::ViewId viewOrder[] = { VIEW_RTT, VIEW_MAIN, VIEW_DEBUG, VIEW_TRANSITION };
    bgfx::setViewOrder(0, 4, viewOrder);

    printf("[BgfxDeviceCore] Initialized %dx%d\n", width, height);
    return true;
}

void BgfxDeviceCore::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (width == m_width && height == m_height) return;
    m_width  = width;
    m_height = height;
    bgfx::reset(uint32_t(width), uint32_t(height), BGFX_RESET_VSYNC);
    setupDefaultViews();
    fprintf(stderr, "[BgfxRenderDevice] Resized to %dx%d\n", width, height);
}

void BgfxDeviceCore::shutdown() {
    CAESURA_ASSERT_MAIN_THREAD();

    // 1. Release all RTT framebuffers while GPU context is alive
    flushAllRTT();
    // Destroy text renderer (GPU resources)


    // 2. Destroy shader programs
    // Shader cleanup handled by BgfxShaderManager dtor


    // 3. Mark shutdown-in-progress to suppress benign D3D11 teardown errors
    g_bgfxDebugCallback.m_shuttingDown = true;

    // 4. Destroy GPU context
    bgfx::shutdown();

    printf("[BgfxRenderDevice] Shutdown complete.\n");
}

void BgfxDeviceCore::beginFrame() {
    CAESURA_ASSERT_MAIN_THREAD();
    // No-op: bgfx::frame() in endFrame() handles frame pacing.
    // The debug-text overlay in VIEW_DEBUG + explicit submit calls
    // in blitTexture/blitViewport drive VIEW_MAIN and VIEW_RTT.
}

void BgfxDeviceCore::endFrame() {
    CAESURA_ASSERT_MAIN_THREAD();
    bgfx::frame();
}

void BgfxDeviceCore::commit_frame() {
    bgfx::frame();
}

void BgfxDeviceCore::setViewRect(uint16_t viewId, uint16_t x, uint16_t y,
                                    uint16_t width, uint16_t height) {
    bgfx::setViewRect(viewId, x, y, width, height);
}

void BgfxDeviceCore::setViewClear(uint16_t viewId, uint16_t flags,
                                     uint32_t rgba, float depth, uint8_t stencil) {
    bgfx::setViewClear(viewId, flags, rgba, depth, stencil);
}

void BgfxDeviceCore::touch(uint16_t viewId) {
    bgfx::touch(viewId);
}

void BgfxDeviceCore::setDebugName(uint16_t viewId, const std::string& name) {
    bgfx::setViewName(viewId, name.c_str());
}

ViewportHandle BgfxDeviceCore::createRenderTarget(int width, int height) {
    ViewportHandle handle;
    handle.id = m_nextHandle++;

    bgfx::TextureHandle tex = bgfx::createTexture2D(
        uint16_t(width), uint16_t(height), false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    if (!bgfx::isValid(tex)) {
        fprintf(stderr, "[BgfxRenderDevice] createRenderTarget: "
                "texture allocation failed\n");
        return ViewportHandle{0};
    }

    bgfx::FrameBufferHandle fb = bgfx::createFrameBuffer(1, &tex, true);
    if (!bgfx::isValid(fb)) {
        fprintf(stderr, "[BgfxRenderDevice] createRenderTarget: "
                "framebuffer allocation failed\n");
        bgfx::destroy(tex);
        return ViewportHandle{0};
    }

    RTTEntry entry;
    entry.fb     = fb;
    entry.tex    = tex;
    entry.viewId = VIEW_RTT;
    m_rttMap[handle.id] = entry;

    printf("[BgfxRenderDevice] RTT %u created (%dx%d)\n",
           handle.id, width, height);
    return handle;
}

void BgfxDeviceCore::destroyRenderTarget(ViewportHandle handle) {
    auto it = m_rttMap.find(handle.id);
    if (it == m_rttMap.end()) return;

    if (bgfx::isValid(it->second.fb)) {
        bgfx::destroy(it->second.fb);
    }
    m_rttMap.erase(it);
    printf("[BgfxRenderDevice] RTT %u destroyed\n", handle.id);
}

bgfx::TextureHandle BgfxDeviceCore::getViewportTexture(ViewportHandle handle) {
    auto it = m_rttMap.find(handle.id);
    if (it != m_rttMap.end() && bgfx::isValid(it->second.tex)) {
        return it->second.tex;
    }
    return BGFX_INVALID_HANDLE;
}

void BgfxDeviceCore::flushAllRTT() {
    // [10.2.67] Release all GPU-side RTT resources while bgfx context is still alive.
    // Must be called before bgfx::shutdown().
    for (auto& [id, entry] : m_rttMap) {
        if (bgfx::isValid(entry.fb)) {
            bgfx::destroy(entry.fb);
        }
    }
    m_rttMap.clear();
}

void BgfxDeviceCore::setupDefaultViews() {
    // -- View RTT (offscreen render target) --
    bgfx::setViewRect(VIEW_RTT, 0, 0, uint16_t(m_width), uint16_t(m_height));
    bgfx::setViewClear(VIEW_RTT, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0x00000000, 1.0f, 0);

    // -- View MAIN (primary compositing) --
    bgfx::setViewRect(VIEW_MAIN, 0, 0, uint16_t(m_width), uint16_t(m_height));
    bgfx::setViewClear(VIEW_MAIN, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0x303030FF, 1.0f, 0);

    // Set ortho projection for MAIN view (pixel coords 0->w, 0->h)
    const bgfx::Caps* caps = bgfx::getCaps();
    float orthoMain[16];
    bx::mtxOrtho(orthoMain, 0.0f, float(m_width), float(m_height), 0.0f,
                 -1.0f, 1.0f, 0.0f,
                 caps ? caps->homogeneousDepth : false,
                 bx::Handedness::Left);
    bgfx::setViewTransform(VIEW_MAIN, nullptr, orthoMain);

    // Set ortho projection for RTT view (same pixel-coord space)
    float orthoRTT[16];
    bx::mtxOrtho(orthoRTT, 0.0f, float(m_width), float(m_height), 0.0f,
                 -1.0f, 1.0f, 0.0f,
                 caps ? caps->homogeneousDepth : false,
                 bx::Handedness::Left);
    bgfx::setViewTransform(VIEW_RTT, nullptr, orthoRTT);

    // -- View DEBUG (engine HUD overlay) --
    bgfx::setViewRect(VIEW_DEBUG, 0, 0, uint16_t(m_width), uint16_t(m_height));
    bgfx::setViewClear(VIEW_DEBUG, BGFX_CLEAR_NONE, 0x00000000, 1.0f, 0);
}

void BgfxDeviceCore::blitViewport(ViewportHandle handle, uint16_t targetView,
                                     float x, float y, float w, float h) {
    auto it = m_rttMap.find(handle.id);
    if (it == m_rttMap.end() || !bgfx::isValid(it->second.tex)) return;

    blitTexture(targetView, it->second.tex, x, y, w, h, 255);
}

bgfx::FrameBufferHandle BgfxDeviceCore::getRttFb(ViewportHandle handle) {
    auto it = m_rttMap.find(handle.id);
    return (it != m_rttMap.end()) ? it->second.fb : BGFX_INVALID_HANDLE;
}

} // namespace Caesura
