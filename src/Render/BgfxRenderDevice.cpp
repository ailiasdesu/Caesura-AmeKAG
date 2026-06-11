 #include "BgfxRenderDevice.h"
#include "ShaderCache.h"
#include "Core/Engine.h"
#include <bgfx/bgfx.h>
// #include <bgfx/embedded_shader.h> -- using bgfx::createShader with raw bytecode instead
#include <bx/math.h>
#include <bgfx/platform.h>
#include <bimg/decode.h>
#include <bx/bx.h>
#include "BgfxShaderManager.h"
#include <bx/readerwriter.h>
#include <bx/error.h>
#include <cstdio>
#include <cstring>


#include "BgfxDeviceCore.h"
#include "BgfxShaderManager.h"
// BgfxDebugCallback + setBgfxShuttingDown extracted to BgfxDebugCallback.h/.cpp

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


void BgfxRenderDevice::beginBatch() {
    m_batching = true;
    m_batchQuads.clear();
}

void BgfxRenderDevice::flushBatch() {
    if (!m_batching || m_batchQuads.empty()) {
        m_batching = false;
        return;
    }

    struct FsVertex { float x, y, u, v; };
    uint32_t quadCount = (uint32_t)m_batchQuads.size();
    uint32_t vertCount = quadCount * 4;
    uint32_t idxCount  = quadCount * 6;

    if (bgfx::getAvailTransientVertexBuffer(vertCount, m_posTexLayout) < vertCount) {
        m_batching = false;
        return;
    }
    if (bgfx::getAvailTransientIndexBuffer(idxCount) < idxCount) {
        m_batching = false;
        return;
    }

    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, vertCount, m_posTexLayout);
    auto* v = (FsVertex*)tvb.data;

    bgfx::TransientIndexBuffer tib;
    bgfx::allocTransientIndexBuffer(&tib, idxCount);
    uint16_t* indices = (uint16_t*)tib.data;

    uint16_t currentView = 0;
    uint32_t quadInBatch = 0;

    // Split into per-texture draw calls within the batch
    // (bgfx requires one texture set per submit)
    uint32_t baseVert = 0;
    uint32_t baseIdx  = 0;

    for (uint32_t qi = 0; qi < quadCount; qi++) {
        auto& q = m_batchQuads[qi];

        // Build quad vertices
        v[qi * 4 + 0] = { q.x,       q.y,       0.0f, 0.0f };
        v[qi * 4 + 1] = { q.x + q.w, q.y,       1.0f, 0.0f };
        v[qi * 4 + 2] = { q.x + q.w, q.y + q.h, 1.0f, 1.0f };
        v[qi * 4 + 3] = { q.x,       q.y + q.h, 0.0f, 1.0f };

        // Build indices
        indices[qi * 6 + 0] = baseVert + 0;
        indices[qi * 6 + 1] = baseVert + 1;
        indices[qi * 6 + 2] = baseVert + 2;
        indices[qi * 6 + 3] = baseVert + 0;
        indices[qi * 6 + 4] = baseVert + 2;
        indices[qi * 6 + 5] = baseVert + 3;
        baseVert += 4;
    }

    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                   | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                           BGFX_STATE_BLEND_INV_SRC_ALPHA);

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setState(state);

    // Submit per-texture: each quad may have different texture
    // For single-texture scenes, we can merge into fewer submits
    uint32_t idxOffset = 0;
    uint32_t vertOffset = 0;

    for (uint32_t qi = 0; qi < quadCount; qi++) {
        auto& q = m_batchQuads[qi];

        // Check if next quad shares same texture ??merge if so
        uint32_t mergeEnd = qi + 1;
        while (mergeEnd < quadCount &&
               m_batchQuads[mergeEnd].tex.idx == q.tex.idx &&
               m_batchQuads[mergeEnd].viewId == q.viewId) {
            mergeEnd++;
        }

        uint32_t mergeCount = mergeEnd - qi;
        uint32_t mergeIdxCount = mergeCount * 6;

        bgfx::setTexture(0, m_shaders->getDefaultSampler(), q.tex);
        // Set opacity as a uniform if needed
        float alpha = q.opacity / 255.0f;
        bgfx::setUniform(m_shaders->getBlendParams(), &alpha, 1);

        // Rebase indices relative to merge group start: bgfx interprets
        // index values as offsets from setVertexBuffer startVertex.
        for (uint32_t m = 0; m < mergeCount; m++) {
            uint32_t localBase = m * 4;
            indices[idxOffset + m*6 + 0] = localBase + 0;
            indices[idxOffset + m*6 + 1] = localBase + 1;
            indices[idxOffset + m*6 + 2] = localBase + 2;
            indices[idxOffset + m*6 + 3] = localBase + 0;
            indices[idxOffset + m*6 + 4] = localBase + 2;
            indices[idxOffset + m*6 + 5] = localBase + 3;
        }

        // Submit the vertex/index subset for this texture group
        bgfx::setVertexBuffer(0, &tvb, vertOffset, mergeCount * 4);
        bgfx::setIndexBuffer(&tib, idxOffset, mergeIdxCount);
        bgfx::submit(q.viewId, m_shaders->getFallbackProgram());

        idxOffset += mergeIdxCount;
        vertOffset += mergeCount * 4;
        qi = mergeEnd - 1;
    }

    m_batchQuads.clear();
    m_batching = false;
}

// ===========================================================================
// Backend preference helpers
// These correspond to the original objective's requirement for explicit
// DX12 / Metal / WebGPU backend stubs. bgfx handles the actual backend
// internally; these helpers expose the selection API.

static bgfx::RendererType::Enum s_preferredBackend = bgfx::RendererType::Direct3D11;

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

bool BgfxRenderDevice::init(void* nativeWindowHandle, int width, int height) {
    // [10.2.22] main-thread-only guarantee 闂傚倸鍊搁崐鎼佸磹閹间礁纾归柟闂寸绾惧綊鏌熼梻瀵割槮缁炬儳婀遍埀顒傛嚀鐎氼參宕崇壕瀣ㄤ汗闁圭儤鍨归崐鐐烘偡濠婂喚妯€鐎殿喗鎮傚浠嬵敇閻斿搫骞愰梻浣规偠閸庮垶宕曢柆宥嗗€堕柍鍝勫暟绾惧ジ鏌熼柇锕€寮炬繛鍫熺矒閺屸€崇暆閳ь剟宕伴弽顓炵畺鐟滄柨鐣锋總鍛婂亜闁告繂瀚▓銉╂⒒閸屾瑧顦﹂柟璇х節瀹曞湱鎲撮崟顒€寮块梺鍦檸閸犳牠鎮″鈧弻鐔告綇妤ｅ啯顎嶉梺绋款儐閸旀瑩骞冨Δ鍛嵍妞ゆ挾鍊姀掳浜滈柕澶涘缁犳绱?architecture enforces, SDL_IsMainThread not in all SDL3 builds
    m_deviceCore->getWidth()  = width;
    m_deviceCore->getHeight() = height;

        // -- bgfx platform setup
        // Register debug callback via bgfx::Init::callback

    // Platform data will be set via initParams.platformData directly

    //                                                     ?bgfx init                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 ?
    bgfx::Init initParams;
    initParams.platformData.nwh = nativeWindowHandle;
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
        fprintf(stderr, "[BgfxRenderDevice] %s init failed; trying auto-select...\n",
                preferredName);
        bgfx::shutdown();
        initParams.type = bgfx::RendererType::Count;
        printf("[BgfxRenderDevice] nwh=%p, w=%d, h=%d, backend=%s\n", nativeWindowHandle, width, height, bgfx::getRendererName(s_preferredBackend));
    if (!bgfx::init(initParams)) {
            fprintf(stderr, "[BgfxRenderDevice] Fatal: bgfx::init failed.\n");
            return false;
        }
    }

    const bgfx::Caps* caps = bgfx::getCaps();
    const char* rendererName = bgfx::getRendererName(caps->rendererType);
    printf("[BgfxRenderDevice] Renderer: %s (%s)\n", rendererName,
           caps->homogeneousDepth ? "homogeneous" : "non-homogeneous");


    // Enable debug text for engine HUD overlay
    bgfx::setDebug(BGFX_DEBUG_TEXT);
    // -- Set up default views --
    setupDefaultViews();
    fprintf(stderr, "[BgfxRenderDevice] Default views OK.\n");

    //                                                     ?Init embedded shader fallback                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     ?
    m_shaders = std::make_unique<BgfxShaderManager>(); m_shaders->initEmbeddedShaders();

    //                                                     ?Explicit View Order                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     
        // Enforce: VIEW_RTT (0) -> VIEW_MAIN (1) -> VIEW_DEBUG (2)
    bgfx::ViewId viewOrder[] = { VIEW_RTT, VIEW_MAIN, VIEW_DEBUG, VIEW_TRANSITION };
    bgfx::setViewOrder(0, 4, viewOrder);

    printf("[BgfxRenderDevice] Initialized %dx%d with 3 views (order: RTT -> MAIN -> DEBUG)\n",
           width, height);
    m_bgfxInitialized = true;
    // Pre-create vertex layout and sampler uniform (one-time, not per-frame lazy)
    m_posTexLayout
        .begin()
        .add(bgfx::Attrib::Position,  2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
    m_shaders->getDefaultSampler() = bgfx::createUniform("s_texture", bgfx::UniformType::Sampler);


    // Initialize embedded text renderer
    m_textRenderer = std::make_unique<TextRenderer>();
    if (!m_textRenderer->init(this)) {
        fprintf(stderr, "[BgfxRenderDevice] TextRenderer init failed.\n");
        m_textRenderer.reset();
    }
    return true;
}

void BgfxRenderDevice::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (width == m_deviceCore->getWidth() && height == m_deviceCore->getHeight()) return;
    m_deviceCore->getWidth()  = width;
    m_deviceCore->getHeight() = height;
    bgfx::reset(uint32_t(width), uint32_t(height), BGFX_RESET_VSYNC);
    setupDefaultViews();
    fprintf(stderr, "[BgfxRenderDevice] Resized to %dx%d\n", width, height);
}

void BgfxRenderDevice::shutdown() { if (m_textRenderer) m_textRenderer.reset(); m_shaders.reset(); m_deviceCore->shutdown(); }; // initEmbeddedShaders extracted

    // 2. Destroy shader programs
    if (bgfx::isValid(m_shaders->getFallbackProgram())) {
        bgfx::destroy(m_shaders->getFallbackProgram());
        m_shaders->getFallbackProgram() = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_shaders->getBlendProgram()))      { bgfx::destroy(m_shaders->getBlendProgram());      m_shaders->getBlendProgram()      = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_shaders->getTransitionProgram())) { bgfx::destroy(m_shaders->getTransitionProgram()); m_shaders->getTransitionProgram() = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_shaders->getVFXProgram()))        { bgfx::destroy(m_shaders->getVFXProgram());        m_shaders->getVFXProgram()        = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_shaders->getBlendParams()))     { bgfx::destroy(m_shaders->getBlendParams());     m_shaders->getBlendParams()     = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_shaders->getTransParams()))     { bgfx::destroy(m_shaders->getTransParams());     m_shaders->getTransParams()     = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_shaders->getVFXParams()))       { bgfx::destroy(m_shaders->getVFXParams());       m_shaders->getVFXParams()       = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_shaders->getStretchProgram()))    { bgfx::destroy(m_shaders->getStretchProgram());    m_shaders->getStretchProgram()    = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_shaders->getAffineProgram()))     { bgfx::destroy(m_shaders->getAffineProgram());     m_shaders->getAffineProgram()     = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_shaders->getStretchParams()))   { bgfx::destroy(m_shaders->getStretchParams());   m_shaders->getStretchParams()   = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_shaders->getAffineParams()))    { bgfx::destroy(m_shaders->getAffineParams());    m_shaders->getAffineParams()    = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_shaders->getDefaultSampler())) {
        bgfx::destroy(m_shaders->getDefaultSampler());
        m_shaders->getDefaultSampler() = BGFX_INVALID_HANDLE;
    }

    // 3. Mark shutdown-in-progress to suppress benign D3D11 teardown errors
    g_bgfxDebugCallback.m_shuttingDown = true;

    // 4. Destroy GPU context
    bgfx::shutdown();
    m_bgfxInitialized = false;
    printf("[BgfxRenderDevice] Shutdown complete.\n");
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

static bgfx::ShaderHandle buildBgfxShader(const uint8_t* bytecode, uint32_t codeSize,
                                            bool fragment, uint8_t numAttrs, const uint16_t* attrIds) {
    // [10.2.3] Shader safety: reject bytecode > 64 KB (SPIR-V/DXBC limit)
    if (codeSize > 65536) {
        fprintf(stderr, "[BgfxRenderDevice] Shader rejected: %u bytes exceeds 64 KB limit.\n", codeSize);
        return BGFX_INVALID_HANDLE;
    }

    const uint16_t uniformCount = 0;

    const uint32_t totalSize = 4 + 4 + 4 + 2
                             + 4 + codeSize + 1
                             + 1 + 2 * numAttrs
                             + 2;

    const bgfx::Memory* mem = bgfx::alloc(totalSize);
    bx::StaticMemoryBlockWriter writer(mem->data, mem->size);
    bx::ErrorAssert err;

    const uint32_t magic = fragment
        ? BX_MAKEFOURCC('F', 'S', 'H', 11)
        : BX_MAKEFOURCC('V', 'S', 'H', 11);
    bx::write(&writer, magic, err);
    bx::write(&writer, uint32_t(0), err);
    bx::write(&writer, uint32_t(0), err);
    bx::write(&writer, uniformCount, err);

    bx::write(&writer, codeSize, err);
    for (uint32_t i = 0; i < codeSize; ++i)
        bx::write(&writer, bytecode[i], err);
    bx::write(&writer, uint8_t(0), err);

    bx::write(&writer, numAttrs, err);
    for (uint8_t i = 0; i < numAttrs; ++i)
        bx::write(&writer, attrIds[i], err);

    bx::write(&writer, uint16_t(0), err);

    return bgfx::createShader(mem);
}


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

void BgfxRenderDevice::beginFrame() {
    CAESURA_ASSERT_MAIN_THREAD();
    // No-op: bgfx::frame() in endFrame() handles frame pacing.
    // The debug-text overlay in VIEW_DEBUG + explicit submit calls
    // in blitTexture/blitViewport drive VIEW_MAIN and VIEW_RTT.
}

void BgfxRenderDevice::endFrame() {
    CAESURA_ASSERT_MAIN_THREAD();
    bgfx::frame();
}

void BgfxRenderDevice::commit_frame() { m_deviceCore->commit_frame(); }


//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T
// View-management pass-throughs
//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T

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


//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T
// createRenderTarget / destroyRenderTarget / blitViewport
//   T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T  T

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
    m_deviceCore->rttMap()[handle.id] = entry;

    printf("[BgfxRenderDevice] RTT %u created (%dx%d)\n",
           handle.id, width, height);
    return handle;
}

void BgfxDeviceCore::destroyRenderTarget(ViewportHandle handle) {
    auto it = m_deviceCore->rttMap().find(handle.id);
    if (it == m_deviceCore->rttMap().end()) return;

    if (bgfx::isValid(it->second.fb)) {
        bgfx::destroy(it->second.fb);
    }
    m_deviceCore->rttMap().erase(it);
    printf("[BgfxRenderDevice] RTT %u destroyed\n", handle.id);
}

void BgfxDeviceCore::blitViewport(ViewportHandle handle, uint16_t targetView,
                                     float x, float y, float w, float h) {
    auto it = m_deviceCore->rttMap().find(handle.id);
    if (it == m_deviceCore->rttMap().end() || !bgfx::isValid(it->second.tex)) return;

    blitTexture(targetView, it->second.tex, x, y, w, h, 255);
}

bgfx::TextureHandle BgfxDeviceCore::getViewportTexture(ViewportHandle handle) {
    auto it = m_deviceCore->rttMap().find(handle.id);
    if (it != m_deviceCore->rttMap().end() && bgfx::isValid(it->second.tex)) {
        return it->second.tex;
    }
    return BGFX_INVALID_HANDLE;
}



void BgfxRenderDevice::blitTexture(uint16_t targetView, uint32_t textureId,
                                    float x, float y, float w, float h, uint8_t opacity) {
    // Batch protocol: when batching, accumulate quads instead of drawing
    if (m_batching) {
        m_batchQuads.push_back({targetView, { static_cast<uint16_t>(textureId) }, x, y, w, h, opacity});
        return;
    }
    bgfx::TextureHandle tex = { uint16_t(textureId) };
    blitTexture(targetView, tex, x, y, w, h, opacity);
}

void BgfxRenderDevice::blitTexture(uint16_t targetView, bgfx::TextureHandle tex,
                                    float x, float y, float w, float h, uint8_t opacity) {
    if (!bgfx::isValid(tex)) return;

    // Ortho projection for screen-space quad
    float ortho[16];
    const bgfx::Caps* caps = bgfx::getCaps();
    bx::mtxOrtho(ortho, 0.0f, float(m_deviceCore->getWidth()), float(m_deviceCore->getHeight()), 0.0f,
                 -1.0f, 1.0f, 0.0f, caps ? caps->homogeneousDepth : false,
                 bx::Handedness::Left);
    bgfx::setViewTransform(targetView, nullptr, ortho);

    // Lazy-init vertex layout
    if (m_posTexLayout.getStride() == 0) {
        m_posTexLayout
            .begin()
            .add(bgfx::Attrib::Position,  2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
    }

    // Lazy-init sampler uniform
    if (!bgfx::isValid(m_shaders->getDefaultSampler())) {
        m_shaders->getDefaultSampler() = bgfx::createUniform("s_texture", bgfx::UniformType::Sampler);
    }

    // Pixel coords -> NDC (passthrough shader bypasses u_viewProj)
    float sw = (float)m_deviceCore->getWidth();
    float sh = (float)m_deviceCore->getHeight();
    float nx  = (x / sw) * 2.0f - 1.0f;
    float ny  = 1.0f - (y / sh) * 2.0f;
    float nx2 = ((x + w) / sw) * 2.0f - 1.0f;
    float ny2 = 1.0f - ((y + h) / sh) * 2.0f;

    struct PosTexVertex { float x, y; float u, v; };

    PosTexVertex quad[4] = {
        { nx,  ny,  0.0f, 0.0f },
        { nx2, ny,  1.0f, 0.0f },
        { nx2, ny2, 1.0f, 1.0f },
        { nx,  ny2, 0.0f, 1.0f },
    };

    bgfx::TransientVertexBuffer tvb;
    if (bgfx::getAvailTransientVertexBuffer(4, m_posTexLayout) < 4) return;
    bgfx::allocTransientVertexBuffer(&tvb, 4, m_posTexLayout);
    bx::memCopy(tvb.data, quad, sizeof(quad));

    uint16_t indices[6] = { 0, 1, 2, 0, 2, 3 };
    bgfx::TransientIndexBuffer tib;
    if (bgfx::getAvailTransientIndexBuffer(6) < 6) return;
    bgfx::allocTransientIndexBuffer(&tib, 6);
    bx::memCopy(tib.data, indices, sizeof(indices));

    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                   | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                           BGFX_STATE_BLEND_INV_SRC_ALPHA);

    // If we have a fallback shader, use it; otherwise texture blit is no-op
    if (!bgfx::isValid(m_shaders->getFallbackProgram())) return;

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setTexture(0, m_shaders->getDefaultSampler(), tex);
    bgfx::setState(state);
    bgfx::submit(targetView, m_shaders->getFallbackProgram());
}


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

void BgfxDeviceCore::setDebugName(uint16_t viewId, const std::string& name) {
    bgfx::setViewName(viewId, name.c_str());
}


// ===========================================================================
//  GPU Effect: Blend -- two-texture blend with selectable mode

// ===========================================================================
//  fillViewport -- render solid-color quad into a viewport RTT framebuffer
// ===========================================================================

void BgfxRenderDevice::fillViewport(ViewportHandle handle,
                                     uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    auto it = m_deviceCore->rttMap().find(handle.id);
    if (it == m_deviceCore->rttMap().end() || !bgfx::isValid(it->second.fb)) return;

    uint16_t vpView = it->second.viewId;
    bgfx::setViewFrameBuffer(vpView, it->second.fb);

    // Create a 1x1 solid-color texture
    uint8_t pixel[4] = { r, g, b, a };
    const bgfx::Memory* mem = bgfx::makeRef(pixel, sizeof(pixel), nullptr, nullptr);
    bgfx::TextureHandle colorTex = bgfx::createTexture2D(1, 1, false, 1,
        bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_POINT, mem);

    if (!bgfx::isValid(colorTex) || !bgfx::isValid(m_shaders->getFallbackProgram())) {
        bgfx::setViewFrameBuffer(vpView, BGFX_INVALID_HANDLE);
        return;
    }

    // Submit fullscreen quad with the solid-color texture
    struct FsVertex { float x, y, u, v; };
    bgfx::TransientVertexBuffer tvb;
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    if (bgfx::getAvailTransientVertexBuffer(4, layout) < 4) {
        bgfx::destroy(colorTex);
        bgfx::setViewFrameBuffer(vpView, BGFX_INVALID_HANDLE);
        return;
    }
    bgfx::allocTransientVertexBuffer(&tvb, 4, layout);
    auto* v = (FsVertex*)tvb.data;
    const bgfx::Caps* caps = bgfx::getCaps();
    v[0] = { -1.0f,  1.0f, 0.0f, 0.0f };
    v[1] = {  1.0f,  1.0f, 1.0f, 0.0f };
    v[2] = {  1.0f, -1.0f, 1.0f, 1.0f };
    v[3] = { -1.0f, -1.0f, 0.0f, 1.0f };

    uint16_t indices[6] = { 0, 1, 2, 0, 2, 3 };
    bgfx::TransientIndexBuffer tib;
    if (bgfx::getAvailTransientIndexBuffer(6) < 6) {
        bgfx::destroy(colorTex);
        bgfx::setViewFrameBuffer(vpView, BGFX_INVALID_HANDLE);
        return;
    }
    bgfx::allocTransientIndexBuffer(&tib, 6);
    bx::memCopy(tib.data, indices, sizeof(indices));

    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                   | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                           BGFX_STATE_BLEND_INV_SRC_ALPHA);

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setTexture(0, m_shaders->getDefaultSampler(), colorTex);
    bgfx::setState(state);
    bgfx::submit(vpView, m_shaders->getFallbackProgram());

    bgfx::destroy(colorTex);
    bgfx::setViewFrameBuffer(vpView, it->second.fb);

    printf("[BgfxRenderDevice] fillViewport #%u: (%d,%d,%d,%d) -> view %u\n",
           handle.id, r, g, b, a, (unsigned)vpView);
}

// ===========================================================================


// ===========================================================================
//  submitFullscreenQuad  helper for GPU effects
// ===========================================================================

// x,y reserved for future 3D RTT offset rendering
static void submitFullscreenQuad(uint16_t viewId, bgfx::ProgramHandle program,
                                  float x, float y, float w, float h,
                                  bgfx::TextureHandle tex, bgfx::UniformHandle sampler,
                                  bgfx::UniformHandle /*params*/, const float* /*paramData*/, uint16_t /*paramVec4s*/) {
    if (!bgfx::isValid(program)) return;

    struct FsVertex { float x, y, u, v; };
    bgfx::TransientVertexBuffer tvb;
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    if (bgfx::getAvailTransientVertexBuffer(4, layout) < 4) return;
    bgfx::allocTransientVertexBuffer(&tvb, 4, layout);
    auto* v = (FsVertex*)tvb.data;

    const bgfx::Caps* caps = bgfx::getCaps();
    v[0] = { -1.0f,  1.0f, 0.0f, 0.0f };
    v[1] = {  1.0f,  1.0f, 1.0f, 0.0f };
    v[2] = {  1.0f, -1.0f, 1.0f, 1.0f };
    v[3] = { -1.0f, -1.0f, 0.0f, 1.0f };

    uint16_t indices[6] = { 0, 1, 2, 0, 2, 3 };
    bgfx::TransientIndexBuffer tib;
    if (bgfx::getAvailTransientIndexBuffer(6) < 6) return;
    bgfx::allocTransientIndexBuffer(&tib, 6);
    bx::memCopy(tib.data, indices, sizeof(indices));

    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                   | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                           BGFX_STATE_BLEND_INV_SRC_ALPHA);

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setState(state);
    if (bgfx::isValid(tex) && bgfx::isValid(sampler))
        bgfx::setTexture(0, sampler, tex);

    bgfx::submit(viewId, program);
}

void BgfxRenderDevice::submitBlend(uint16_t viewId, bgfx::TextureHandle baseTex,
                                    bgfx::TextureHandle blendTex, int mode,
                                    float baseAlpha, float blendAlpha, float globalAlpha) {
    if (!bgfx::isValid(m_shaders->getBlendProgram())) {
        static bool once = false;
        if (!once) { fprintf(stderr, "[BgfxRenderDevice] submitBlend: blend program not loaded.\n"); once = true; }
        return;
    }

    bgfx::setTexture(0, m_shaders->getDefaultSampler(), baseTex);
    bgfx::setTexture(1, m_shaders->getDefaultSampler(), blendTex);

    float params[8] = { baseAlpha, blendAlpha, globalAlpha, (float)mode, 0, 0, 0, 0 };
    bgfx::setUniform(m_shaders->getBlendParams(), params, 2);

    submitFullscreenQuad(viewId, m_shaders->getBlendProgram(), 0, 0, (float)m_deviceCore->getWidth(), (float)m_deviceCore->getHeight(), BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, nullptr, 0);
}

// ===========================================================================
//  GPU Effect: Transition ?? crossfade / rule / wipe between two textures
// ===========================================================================

// Spec [10.2.25]: @Beta 闂?Pre-bake rule images into a LUT texture atlas for batch
// transition rendering. Currently each transition passes its rule texture
// individually via texture slot 2. A pre-baked atlas would reduce draw calls.
void BgfxRenderDevice::submitTransition(uint16_t viewId, bgfx::TextureHandle fromTex,
                                         bgfx::TextureHandle toTex,
                                         bgfx::TextureHandle ruleTex,
                                         int method, float progress) {
    if (!bgfx::isValid(m_shaders->getTransitionProgram())) {
        static bool once = false;
        if (!once) { fprintf(stderr, "[BgfxRenderDevice] submitTransition: transition program not loaded.\n"); once = true; }
        return;
    }

    bgfx::setTexture(0, m_shaders->getDefaultSampler(), fromTex);
    bgfx::setTexture(1, m_shaders->getDefaultSampler(), toTex);
    if (bgfx::isValid(ruleTex))
        bgfx::setTexture(2, m_shaders->getDefaultSampler(), ruleTex);

    float params[4] = { progress, (float)method, 0, 0 };
    bgfx::setUniform(m_shaders->getTransParams(), params, 1);

    submitFullscreenQuad(viewId, m_shaders->getTransitionProgram(), 0, 0, (float)m_deviceCore->getWidth(), (float)m_deviceCore->getHeight(), BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, nullptr, 0);
}

// ===========================================================================
//  GPU Effect: VFX ?? fade / blur / quake post-processing
// ===========================================================================

void BgfxRenderDevice::submitVFX(uint16_t viewId, bgfx::TextureHandle srcTex,
                                  int effect, float fadeAlpha,
                                  float fadeR, float fadeG, float fadeB,
                                  float blurRadius, float quakeX, float quakeY) {
    if (!bgfx::isValid(m_shaders->getVFXProgram())) {
        static bool once = false;
        if (!once) { fprintf(stderr, "[BgfxRenderDevice] submitVFX: VFX program not loaded.\n"); once = true; }
        return;
    }

    bgfx::setTexture(0, m_shaders->getDefaultSampler(), srcTex);

    // VFXParams cbuffer layout:
    // Vec4[0] = u_color (r,g,b, fadeAlpha)
    // Vec4[1] = u_blurR (x,y) + u_qx + u_qy
    // Vec4[2] = u_effect + padding
    // VFXParams is a single Vec4[3] uniform ?? set all 12 floats at once
    float vfxData[12] = {
        fadeR, fadeG, fadeB, fadeAlpha,
        blurRadius, blurRadius, quakeX, quakeY,
        (float)effect, 0, 0, 0
    };
    bgfx::setUniform(m_shaders->getVFXParams(), vfxData, 3);

    submitFullscreenQuad(viewId, m_shaders->getVFXProgram(), 0, 0, (float)m_deviceCore->getWidth(), (float)m_deviceCore->getHeight(), srcTex, m_shaders->getDefaultSampler(), BGFX_INVALID_HANDLE, nullptr, 0);
}

// ===========================================================================
//  GPU Transform: Stretch Blit (filtered copy with src/dst rects)
// ===========================================================================

void BgfxRenderDevice::stretchBlt(uint16_t targetView, uint32_t dstTexId,
                                   float dx, float dy, float dw, float dh,
                                   uint32_t srcTexId,
                                   float sx, float sy, float sw, float sh,
                                   int /*filterType*/) {
    // This is called from RenderBinding which already did the texture lookup.
    // srcTexId here is actually the raw bgfx TextureHandle idx (by convention).
    bgfx::TextureHandle srcTex = { uint16_t(srcTexId) };
    (void)dstTexId;

    if (!bgfx::isValid(srcTex)) {
        fprintf(stderr, "[BgfxRenderDevice] stretchBlt: invalid src tex\n");
        return;
    }

    float hw = (float)m_deviceCore->getWidth()  * 0.5f;
    float hh = (float)m_deviceCore->getHeight() * 0.5f;
    float l = (dx / hw) - 1.0f;
    float r = ((dx + dw) / hw) - 1.0f;
    float t = 1.0f - (dy / hh);
    float b = 1.0f - ((dy + dh) / hh);

    struct FsVertex { float x, y, u, v; };
    bgfx::TransientVertexBuffer tvb;
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    if (bgfx::getAvailTransientVertexBuffer(4, layout) < 4) return;
    bgfx::allocTransientVertexBuffer(&tvb, 4, layout);
    auto* v = (FsVertex*)tvb.data;

    v[0] = { l, t, sx,      sy };
    v[1] = { r, t, sx + sw, sy };
    v[2] = { r, b, sx + sw, sy + sh };
    v[3] = { l, b, sx,      sy + sh };

    uint16_t indices[6] = { 0, 1, 2, 0, 2, 3 };
    bgfx::TransientIndexBuffer tib;
    if (bgfx::getAvailTransientIndexBuffer(6) < 6) return;
    bgfx::allocTransientIndexBuffer(&tib, 6);
    bx::memCopy(tib.data, indices, sizeof(indices));

    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                   | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                           BGFX_STATE_BLEND_INV_SRC_ALPHA);

    float stretchData[4] = { sx, sy, sw, sh };
    bgfx::setUniform(m_shaders->getStretchParams(), stretchData, 1);

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setState(state);
    bgfx::setTexture(0, m_shaders->getDefaultSampler(), srcTex);

    bgfx::ProgramHandle prog = bgfx::isValid(m_shaders->getStretchProgram()) ? m_shaders->getStretchProgram() : m_shaders->getFallbackProgram();
    bgfx::submit(targetView, prog);
}

// ===========================================================================
//  GPU Transform: Affine Blit (2D affine matrix transform)
// ===========================================================================

void BgfxRenderDevice::affineBlt(uint16_t targetView, uint32_t dstTexId,
                                  float dx, float dy, float dw, float dh,
                                  uint32_t srcTexId,
                                  float sx, float sy, float sw, float sh,
                                  const float matrix[6]) {
    bgfx::TextureHandle srcTex = { uint16_t(srcTexId) };
    (void)dstTexId;

    if (!bgfx::isValid(srcTex)) {
        fprintf(stderr, "[BgfxRenderDevice] affineBlt: invalid src tex\n");
        return;
    }

    float hw = (float)m_deviceCore->getWidth()  * 0.5f;
    float hh = (float)m_deviceCore->getHeight() * 0.5f;
    float l = (dx / hw) - 1.0f;
    float r = ((dx + dw) / hw) - 1.0f;
    float t = 1.0f - (dy / hh);
    float b = 1.0f - ((dy + dh) / hh);

    struct FsVertex { float x, y, u, v; };
    bgfx::TransientVertexBuffer tvb;
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    if (bgfx::getAvailTransientVertexBuffer(4, layout) < 4) return;
    bgfx::allocTransientVertexBuffer(&tvb, 4, layout);
    auto* v = (FsVertex*)tvb.data;

    v[0] = { l, t, sx,      sy };
    v[1] = { r, t, sx + sw, sy };
    v[2] = { r, b, sx + sw, sy + sh };
    v[3] = { l, b, sx,      sy + sh };

    uint16_t indices[6] = { 0, 1, 2, 0, 2, 3 };
    bgfx::TransientIndexBuffer tib;
    if (bgfx::getAvailTransientIndexBuffer(6) < 6) return;
    bgfx::allocTransientIndexBuffer(&tib, 6);
    bx::memCopy(tib.data, indices, sizeof(indices));

    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                   | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                           BGFX_STATE_BLEND_INV_SRC_ALPHA);

    // Shader layout: u_affineParams[0]={a,c,tx,0}, u_affineParams[1]={b,d,ty,0}
    float affineData[16] = {
        matrix[0], matrix[2], matrix[4], 0.0f,   // {a, c, tx, 0}
        matrix[1], matrix[3], matrix[5], 0.0f,   // {b, d, ty, 0}
        sx, sy, sw, sh,
        0.0f, 0.0f, 0.0f, 0.0f
    };
    bgfx::setUniform(m_shaders->getAffineParams(), affineData, 4);
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setState(state);
    bgfx::setTexture(0, m_shaders->getDefaultSampler(), srcTex);

    bgfx::ProgramHandle prog = bgfx::isValid(m_shaders->getAffineProgram()) ? m_shaders->getAffineProgram() : m_shaders->getFallbackProgram();
    bgfx::submit(targetView, prog);
}

} // namespace Caesura
// -- DeviceCore delegation --------------------------------------------
void BgfxRenderDevice::beginFrame() { m_deviceCore->beginFrame(); }
void BgfxRenderDevice::endFrame() { m_deviceCore->endFrame(); }
void BgfxRenderDevice::commit_frame() { m_deviceCore->commit_frame(); }
void BgfxRenderDevice::setViewRect(uint16_t v, uint16_t x, uint16_t y, uint16_t w, uint16_t h) { m_deviceCore->setViewRect(v,x,y,w,h); }
void BgfxRenderDevice::setViewClear(uint16_t v, uint16_t f, uint32_t c, float d, uint8_t s) { m_deviceCore->setViewClear(v,f,c,d,s); }
void BgfxRenderDevice::touch(uint16_t v) { m_deviceCore->touch(v); }
void BgfxRenderDevice::setDebugName(uint16_t v, const std::string& n) { m_deviceCore->setDebugName(v,n.c_str()); }
ViewportHandle BgfxRenderDevice::createRenderTarget(int w, int h) { return m_deviceCore->createRenderTarget(w,h); }
void BgfxRenderDevice::destroyRenderTarget(ViewportHandle h) { m_deviceCore->destroyRenderTarget(h); }
void BgfxRenderDevice::blitViewport(ViewportHandle h, uint16_t v, float x, float y, float w, float h2) { m_deviceCore->blitViewport(h,v,x,y,w,h2); }
bgfx::TextureHandle BgfxRenderDevice::getViewportTexture(ViewportHandle h) { return m_deviceCore->getViewportTexture(h); }
