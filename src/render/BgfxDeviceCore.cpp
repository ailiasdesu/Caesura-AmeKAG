#include "BgfxDeviceCore.h"
#include "BgfxDebugCallback.h"
#include "../debug/api/DebugLog.h"   // P1-6: api header instead of concrete DebugManager.h
#include "../di/api/ThreadAssert.h"
#include <bx/math.h>
#include <bx/bx.h>
#include <cstdio>
#include <cstring>
#if defined(__ANDROID__)
static void* s_overrideGLContext = nullptr;  // set via setOverrideGLContext
#endif

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
    } else if (strcmp(name, "gles") == 0 || strcmp(name, "opengles") == 0) {
        s_preferredBackend = bgfx::RendererType::OpenGLES;
    } else {
        DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                  "[BgfxRenderDevice] Unknown backend: %s", name);
        return false;
    }
    printf("[BgfxRenderDevice] Preferred backend set to: %s\n", name);
    return true;
}

const char* BgfxDeviceCore::getBackendName() const {
    return bgfx::getRendererName(bgfx::getCaps()->rendererType);
}

#if defined(__ANDROID__)
void BgfxDeviceCore::setOverrideGLContext(void* ctx) { s_overrideGLContext = ctx; }
#endif

bool BgfxDeviceCore::init(void* nativeWindowHandle, int width, int height) {
    // [10.2.22] main-thread-only guarantee: bgfx must be driven from
    // the thread that created the context. SDL_IsMainThread is not
    // available in all SDL3 builds, so the engine relies on the
    // existing owner-thread discipline instead.
    m_width  = width;
    m_height = height;
    m_bgfxInitialized = false;
    m_shutdownComplete = false;

        // -- bgfx platform setup
        // Register debug callback via bgfx::Init::callback

    // Platform data will be set via initParams.platformData directly

    // -- bgfx platform setup (native window handle from SDL) --
    bgfx::Init initParams;
    initParams.platformData.nwh = nativeWindowHandle;
#if defined(__ANDROID__)
    // Device-day: reuse the SDL-owned EGL context (bgfx's own EGL bootstrap
    // fails silently on this device; providing it externally is the fix).
    initParams.platformData.context = s_overrideGLContext;
#endif
    initParams.type     = s_preferredBackend;
    initParams.vendorId = BGFX_PCI_ID_NONE;
    initParams.resolution.width  = uint32_t(width);
    initParams.resolution.height = uint32_t(height);
    initParams.resolution.reset  = BGFX_RESET_VSYNC;

    // Enable debug text for engine HUD overlay

    initParams.profile  = false;
    initParams.callback = &g_bgfxDebugCallback;

    printf("[BgfxRenderDevice] nwh=%p, w=%d, h=%d, backend=%s\n", nativeWindowHandle, width, height, bgfx::getRendererName(s_preferredBackend));
    if (!bgfx::init(initParams)) {
        // Fallback: let bgfx auto-select best renderer
        const char* preferredName = bgfx::getRendererName(s_preferredBackend);
        DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                  "[BgfxRenderDevice] %s init failed; trying auto-select...", preferredName);
        initParams.type = bgfx::RendererType::Count;
        printf("[BgfxRenderDevice] nwh=%p, w=%d, h=%d, backend=auto-select\n",
               nativeWindowHandle, width, height);
    if (!bgfx::init(initParams)) {
            DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                      "[BgfxRenderDevice] Fatal: bgfx::init failed.");
            return false;
        }
    }
    m_bgfxInitialized = true;

    const bgfx::Caps* caps = bgfx::getCaps();
    const char* rendererName = bgfx::getRendererName(caps->rendererType);
    printf("[BgfxRenderDevice] Renderer: %s (%s)\n", rendererName,
           caps->homogeneousDepth ? "homogeneous" : "non-homogeneous");


    // Enable debug text for engine HUD overlay
    bgfx::setDebug(BGFX_DEBUG_TEXT);
    setupDefaultViews();
    DEBUG_INFO(SubSys::Render, ErrCode::Ok,
               "[BgfxRenderDevice] Default views OK.");

    // -- Embedded shader fallback (initEmbeddedShaders is called by
    // BgfxRenderDevice::init after this point) --

    // -- Explicit view order (RTT -> MAIN -> DEBUG -> TRANSITION) --
        // Enforce: VIEW_RTT (0) -> VIEW_MAIN (1) -> VIEW_DEBUG (2)
    bgfx::ViewId viewOrder[] = { VIEW_RTT, VIEW_MAIN, VIEW_POSTFX, VIEW_DEBUG, VIEW_TRANSITION };
    bgfx::setViewOrder(0, 5, viewOrder);

    printf("[BgfxRenderDevice] Initialized %dx%d with 3 views (order: RTT -> MAIN -> DEBUG)\n",
           width, height);
// Pre-create vertex layout and sampler uniform (one-time, not per-frame lazy)


    // Initialize embedded text renderer
    return true;
}

void BgfxDeviceCore::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (width == m_width && height == m_height) return;
    m_width  = width;
    m_height = height;
    setupDefaultViews();
    DEBUG_INFO(SubSys::Render, ErrCode::Ok,
               "[BgfxRenderDevice] Resized to %dx%d", width, height);
}

void BgfxDeviceCore::shutdown() {
    CAESURA_ASSERT_MAIN_THREAD();
    if (m_shutdownComplete) return;
    m_shutdownComplete = true;
    if (!m_bgfxInitialized) return;
    // 1. Release all RTT framebuffers while GPU context is alive
    flushAllRTT();
    // Destroy text renderer (GPU resources)

    // 2. Destroy shader programs



















    // 3. Mark shutdown-in-progress to suppress benign D3D11 teardown errors
    g_bgfxDebugCallback.m_shuttingDown = true;

    // 4. Destroy GPU context
    bgfx::shutdown();
    m_bgfxInitialized = false;
printf("[BgfxRenderDevice] Shutdown complete.\n");
}

void BgfxDeviceCore::beginFrame() {
    if (!m_bgfxInitialized) return;
    CAESURA_ASSERT_MAIN_THREAD();
    // Screen-offset pan: shift VIEW_MAIN's rect by the camera/quakes offset
    // (clamped so the view never leaves the backbuffer entirely).
    if (m_screenOffsetX != 0 || m_screenOffsetY != 0) {
        // The bgfx rect API is uint16 -- negative offsets would wrap to
        // ~65436 and intersect to a zero-area view (blank frame). Clamp
        // negatives to 0 (pan-right/down only via this path).
        const int32_t limX = static_cast<int32_t>(m_width) - 1;
        const int32_t limY = static_cast<int32_t>(m_height) - 1;
        const int32_t vx = std::max<int32_t>(0, std::min<int32_t>(m_screenOffsetX, limX));
        const int32_t vy = std::max<int32_t>(0, std::min<int32_t>(m_screenOffsetY, limY));
        bgfx::setViewRect(VIEW_MAIN, static_cast<uint16_t>(vx),
                          static_cast<uint16_t>(vy),
                          static_cast<uint16_t>(m_width),
                          static_cast<uint16_t>(m_height));
    } else {
        bgfx::setViewRect(VIEW_MAIN, 0, 0, static_cast<uint16_t>(m_width),
                          static_cast<uint16_t>(m_height));
    }
    // The debug-text overlay in VIEW_DEBUG + explicit submit calls
    // in blitTexture/blitViewport drive VIEW_MAIN and VIEW_RTT.
}

void BgfxDeviceCore::endFrame() {
    if (!m_bgfxInitialized) return;
    CAESURA_ASSERT_MAIN_THREAD();
    bgfx::frame();
    static int s_probe = 0;
    if (s_probe++ < 40) {
        const bgfx::Stats* st = bgfx::getStats();
        fprintf(stderr, "[BGFX-PROBE] endFrame draw=%u\n", (unsigned)st->numDraw);
    }
}

void BgfxDeviceCore::commit_frame() {
    if (!m_bgfxInitialized) return;
    bgfx::frame();
    static int s_probe2 = 0;
    if (s_probe2++ < 40) {
        const bgfx::Stats* st = bgfx::getStats();
        fprintf(stderr, "[BGFX-PROBE] commit draw=%u\n", (unsigned)st->numDraw);
    }
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
        DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                  "[BgfxRenderDevice] createRenderTarget: ""texture allocation failed");
        return ViewportHandle{0};
    }

    bgfx::FrameBufferHandle fb = bgfx::createFrameBuffer(1, &tex, true);
    if (!bgfx::isValid(fb)) {
        DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                  "[BgfxRenderDevice] createRenderTarget: ""framebuffer allocation failed");
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
    if (bgfx::isValid(m_solidPixel)) {
        bgfx::destroy(m_solidPixel);
        m_solidPixel = BGFX_INVALID_HANDLE;
        m_solidPixelKey = 0;
    }
}

bgfx::TextureHandle BgfxDeviceCore::getSolidPixel(uint8_t r, uint8_t g,
                                                  uint8_t b, uint8_t a) {
    const uint32_t key = (uint32_t(r) << 24) | (uint32_t(g) << 16)
                       | (uint32_t(b) << 8) | uint32_t(a);
    if (bgfx::isValid(m_solidPixel) && key == m_solidPixelKey) {
        return m_solidPixel;
    }
    if (bgfx::isValid(m_solidPixel)) bgfx::destroy(m_solidPixel);
    const uint8_t pixel[4] = { r, g, b, a };
    const bgfx::Memory* mem = bgfx::makeRef(pixel, sizeof(pixel), nullptr, nullptr);
    m_solidPixel = bgfx::createTexture2D(1, 1, false, 1,
        bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_POINT, mem);
    m_solidPixelKey = key;
    return m_solidPixel;
}

bgfx::FrameBufferHandle BgfxDeviceCore::getRttFb(ViewportHandle handle) {
    auto it = m_rttMap.find(handle.id);
    if (it != m_rttMap.end()) return it->second.fb;
    return BGFX_INVALID_HANDLE;
}
} // namespace Caesura