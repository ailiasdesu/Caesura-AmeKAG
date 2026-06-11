 #include "BgfxRenderDevice.h"
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
class BgfxDebugCallback : public bgfx::CallbackI {
public:
    bool m_shuttingDown = false;

    void fatal(const char* _filePath, uint16_t _line, bgfx::Fatal::Enum _code, const char* _str) override {
        // Downgrade all bgfx FATAL to WARN: D3D11 shader creation failures
        // are pre-existing bgfx issues and must not terminate the engine.
        BX_UNUSED(_code);
        if (!m_shuttingDown) {
            fprintf(stderr, "[bgfx WARN] %s(%d): %s\n", _filePath, (int)_line, _str);
        }
        // During shutdown: suppress noise from tearing down GPU resources
    }
    void traceVargs(const char* _filePath, uint16_t _line, const char* _format, va_list _argList) override {
        char buf[2048];
        vsnprintf(buf, sizeof(buf), _format, _argList);
        fprintf(stderr, "[bgfx] %s(%d): %s\n", _filePath, (int)_line, buf);
    }
    void profilerBegin(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerEnd() override {}
    uint32_t cacheReadSize(uint64_t) override { return 0; }
    bool cacheRead(uint64_t, void*, uint32_t) override { return false; }
    void cacheWrite(uint64_t, const void*, uint32_t) override {}
    void screenShot(const char*, uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, const void*, uint32_t, bool) override {}
    void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) override {}
    void captureEnd() override {}
    void captureFrame(const void*, uint32_t) override {}
};

static BgfxDebugCallback s_debugCallback;


// -- Shutdown coordination: called by Engine::shutdown() before GPU teardown --
void setBgfxShuttingDown(bool shuttingDown) {
    s_debugCallback.m_shuttingDown = shuttingDown;
}

namespace Caesura {

BgfxRenderDevice::~BgfxRenderDevice() {
    shutdown();
}



void BgfxRenderDevice::flushAllRTT() {
    // [10.2.67] Release all GPU-side RTT resources while bgfx context is still alive.
    // Must be called before bgfx::shutdown().
    for (auto& [id, entry] : m_rttMap) {
        if (bgfx::isValid(entry.fb)) {
            bgfx::destroy(entry.fb);
        }
    }
    m_rttMap.clear();
}

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
    m_posTexLayout
        .begin()
        .add(bgfx::Attrib::Position,  2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
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

void BgfxRenderDevice::blitViewport(ViewportHandle handle, uint16_t targetView,
                                     float x, float y, float w, float h) {
    auto it = m_rttMap.find(handle.id);
    if (it == m_rttMap.end() || !bgfx::isValid(it->second.tex)) return;

    blitTexture(targetView, it->second.tex, x, y, w, h, 255);
}

bgfx::TextureHandle BgfxRenderDevice::getViewportTexture(ViewportHandle handle) {
    auto it = m_rttMap.find(handle.id);
    if (it != m_rttMap.end() && bgfx::isValid(it->second.tex)) {
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
        // m_texSampler now created inside BgfxShaderManager::initEmbeddedShaders()
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

void BgfxRenderDevice::setDebugName(uint16_t viewId, const std::string& name) {
    bgfx::setViewName(viewId, name.c_str());
}


// ===========================================================================
//  GPU Effect: Blend -- two-texture blend with selectable mode

// ===========================================================================
//  fillViewport -- render solid-color quad into a viewport RTT framebuffer
// ===========================================================================

void BgfxRenderDevice::fillViewport(ViewportHandle handle,
                                     uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    auto it = m_rttMap.find(handle.id);
    if (it == m_rttMap.end() || !bgfx::isValid(it->second.fb)) return;

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

