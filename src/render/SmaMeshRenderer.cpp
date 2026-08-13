#include "SmaMeshRenderer.h"
#include "BgfxShaderManager.h"
#include "EmbeddedShaders.h"
#include "SmaSkinner.h"
#include <bgfx/bgfx.h>
#include <bx/bx.h>
#include <bx/error.h>
#include <bx/readerwriter.h>
#include <cstdio>
#include <cstring>

namespace Caesura {

namespace {

constexpr uint32_t kMaxBones = 64;
constexpr uint8_t kUniformFragmentBit = 0x10;

// ---------------------------------------------------------------------------
// Build a bgfx shader binary (VSH/FSH/CSH v11) around raw backend code
// (DXBC for D3D11, GLSL source text for GL). Mirrors
// BgfxShaderManager::buildBgfxShader but supports compute magic and an
// explicit uniform table (D3D11 cbuffer layout = regIndex * 16 bytes).
// ---------------------------------------------------------------------------
struct ShaderUniformMeta {
    const char* name;
    uint8_t type;  // raw type byte (Vec4, optional fragment bit)
    uint8_t num;
    uint16_t regIndex;
    uint16_t regCount;
};

bgfx::ShaderHandle buildShaderBinary(const uint8_t* code, uint32_t codeSize,
                                     char type,
                                     const ShaderUniformMeta* uniforms,
                                     uint32_t uniformCount,
                                     uint8_t numAttrs,
                                     const uint16_t* attrIds) {
    if (!code || codeSize == 0 || codeSize > 65536) {
        fprintf(stderr, "[SmaMeshRenderer] Shader rejected: %u bytes\n",
                codeSize);
        return BGFX_INVALID_HANDLE;
    }
    uint32_t uniformBytes = 0;
    uint32_t cbSize = 0;  // D3D11 per-shader constant buffer bytes
    for (uint32_t i = 0; i < uniformCount; ++i) {
        uniformBytes += 1 + static_cast<uint32_t>(std::strlen(uniforms[i].name))
                      + 1 + 1 + 2 + 2 + 2 + 2;
        cbSize += uint32_t(uniforms[i].regCount) * 16;
    }
    const uint32_t totalSize = 4 + 4 + 4 + 2 + uniformBytes + 4 + codeSize + 1
                             + 1 + 2 * numAttrs + 2;
    const bgfx::Memory* mem = bgfx::alloc(totalSize);
    if (!mem) return BGFX_INVALID_HANDLE;
    bx::StaticMemoryBlockWriter writer(mem->data, mem->size);
    bx::ErrorAssert err;
    const uint32_t magic = BX_MAKEFOURCC(type, 'S', 'H', 0x0B);
    bx::write(&writer, magic, err);
    bx::write(&writer, uint32_t(0), err);  // hashIn
    bx::write(&writer, uint32_t(0), err);  // hashOut
    bx::write(&writer, uint16_t(uniformCount), err);
    for (uint32_t i = 0; i < uniformCount; ++i) {
        const ShaderUniformMeta& u = uniforms[i];
        const uint8_t nameSize = uint8_t(std::strlen(u.name));
        bx::write(&writer, nameSize, err);
        bx::write(&writer, u.name, nameSize, err);
        bx::write(&writer, u.type, err);
        bx::write(&writer, u.num, err);
        bx::write(&writer, u.regIndex, err);
        bx::write(&writer, u.regCount, err);
        bx::write(&writer, uint16_t(0), err);  // texInfo
        bx::write(&writer, uint16_t(0), err);  // texFormat
    }
    bx::write(&writer, codeSize, err);
    bx::write(&writer, code, codeSize, err);
    bx::write(&writer, uint8_t(0), err);
    bx::write(&writer, numAttrs, err);
    for (uint8_t i = 0; i < numAttrs; ++i) bx::write(&writer, attrIds[i], err);
    // D3D11: the per-shader constant buffer byte size (0 = no cbuffer).
    bx::write(&writer, uint16_t(cbSize), err);
    return bgfx::createShader(mem);
}

struct Bytecode {
    const uint8_t* data = nullptr;
    size_t size = 0;
};

} // namespace

// ---------------------------------------------------------------------------
// init / lifecycle
// ---------------------------------------------------------------------------

SmaMeshRenderer::SmaMeshRenderer() = default;

SmaMeshRenderer::~SmaMeshRenderer() {
    if (bgfx::isValid(m_boneBuffer)) bgfx::destroy(m_boneBuffer);
    if (bgfx::isValid(m_skinProgram)) bgfx::destroy(m_skinProgram);
}

void SmaMeshRenderer::init() {
    if (m_initialized) return;
    // bgfx not up (headless / CI): stay inert; every op becomes a no-op.
    if (bgfx::getRendererType() == bgfx::RendererType::Noop) return;

    m_layout
        .begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
    // Compute input layout: pos(2) + uv(2) + bone0/bone1(2) + w0/w1(2).
    m_skinLayout
        .begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord1, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord2, 2, bgfx::AttribType::Float)
        .end();

    m_shaders = std::make_unique<BgfxShaderManager>();
    m_shaders->initEmbeddedShaders();

    // S5: build the compute skin pass + the skin draw program when the
    // backend supports compute (D3D11/D3D12 and GL 4.3+).
    const bgfx::RendererType::Enum renderer = bgfx::getRendererType();
    const bool isD3D = renderer == bgfx::RendererType::Direct3D11
                    || renderer == bgfx::RendererType::Direct3D12;
    const bool isGL = renderer == bgfx::RendererType::OpenGL
                   || renderer == bgfx::RendererType::OpenGLES;
    const uint64_t caps = bgfx::getCaps()->supported;

    if ((caps & BGFX_CAPS_COMPUTE) && (isD3D || isGL)) {
        Bytecode cs = isD3D
            ? Bytecode{ kEmbeddedCS_SkinDXBC, kEmbeddedCS_SkinDXBC_size }
            : Bytecode{ kEmbeddedCS_SkinGL, kEmbeddedCS_SkinGL_size };
        bgfx::ShaderHandle csh = buildShaderBinary(
            cs.data, uint32_t(cs.size), 'C', nullptr, 0, 0, nullptr);
        if (bgfx::isValid(csh)) {
            m_skinProgram = bgfx::createProgram(csh, true);
        }
        if (bgfx::isValid(m_skinProgram)) {
            // Shared bone transform buffer: 64 bones x vec4.
            bgfx::VertexLayout boneLayout;
            boneLayout
                .begin()
                .add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
                .end();
            // 64 bones + draw transform slot + view size slot.
            m_boneBuffer = bgfx::createDynamicVertexBuffer(
                kMaxBones + 2, boneLayout, BGFX_BUFFER_COMPUTE_READ);
            if (!bgfx::isValid(m_boneBuffer)) {
                bgfx::destroy(m_skinProgram);
                m_skinProgram = BGFX_INVALID_HANDLE;
            }
        }

        // The compute skin pass writes the FINAL NDC positions (draw
        // transform + view size ride in bone buffer slots 64/65), so the
        // draw reuses the engine's proven passthrough program — no extra
        // vertex shader or uniforms needed.
        if (!bgfx::isValid(m_skinProgram)) {
            fprintf(stderr,
                    "[SmaMeshRenderer] S5 GPU skinning unavailable "
                    "(compute program build failed); using CPU skinner.\n");
        }
    }

    m_initialized = true;
}

void SmaMeshRenderer::setSkinMode(SkinMode mode) {
    m_skinMode = mode;
}

SmaMeshRenderer::MeshEntry* SmaMeshRenderer::find(MeshHandle handle) {
    for (auto& entry : m_meshes) {
        if (entry.handle == handle) return &entry;
    }
    return nullptr;
}

bool SmaMeshRenderer::useGpuSkin(const MeshEntry& entry) const {
    if (m_skinMode == SkinMode::Cpu) return false;
    const bool capable = entry.gpuSkinReady
        && bgfx::isValid(m_skinProgram)
        && bgfx::isValid(m_boneBuffer);
    if (m_skinMode == SkinMode::Gpu && !capable) {
        if (!m_skinWarningShown) {
            fprintf(stderr,
                    "[SmaMeshRenderer] GPU skinning requested but "
                    "unavailable; falling back to CPU.\n");
            m_skinWarningShown = true;
        }
        return false;
    }
    return capable;  // Auto: capability-based
}

// ---------------------------------------------------------------------------
// IMeshRenderer
// ---------------------------------------------------------------------------

MeshHandle SmaMeshRenderer::createMesh(const SMAMesh& mesh) {
    if (!m_initialized) init();
    if (!m_initialized) return {};  // deferred-gpu: no GPU -> invalid handle
    if (mesh.vertices.empty() || mesh.indices.empty()
        || mesh.indices.size() % 3 != 0) {
        return {};
    }

    MeshEntry entry;
    entry.handle = MeshHandle{ m_nextId++ };
    entry.mesh = mesh;
    // CPU side: initial skinned copy = identity pose (raw vertices).
    entry.skinned.resize(mesh.vertices.size());
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const SMAMeshVertex& v = mesh.vertices[i];
        entry.skinned[i] = { v.x, v.y, v.u, v.v };
    }
    entry.ib = bgfx::createIndexBuffer(
        bgfx::makeRef(mesh.indices.data(),
                      static_cast<uint32_t>(mesh.indices.size() * sizeof(uint16_t))));
    entry.gpuReady = bgfx::isValid(entry.ib);

    // S5: compute input/output buffers when the skin pipeline is usable.
    // The INPUT is a STATIC buffer (created with its data): D3D11 forbids
    // USAGE_DYNAMIC with a shader-resource bind, which bgfx's dynamic
    // compute-read buffers use — they silently fail to render on D3D11.
    const uint64_t caps = bgfx::getCaps()->supported;
    if (entry.gpuReady && (caps & BGFX_CAPS_COMPUTE)
        && bgfx::isValid(m_skinProgram) && bgfx::isValid(m_boneBuffer)) {
        const uint32_t vcount = static_cast<uint32_t>(mesh.vertices.size());
        std::vector<float> data;
        data.reserve(vcount * 8);
        for (const SMAMeshVertex& v : mesh.vertices) {
            data.push_back(v.x);
            data.push_back(v.y);
            data.push_back(v.u);
            data.push_back(v.v);
            data.push_back(static_cast<float>(v.bone0));
            data.push_back(static_cast<float>(v.bone1));
            data.push_back(v.w0);
            data.push_back(v.w1);
        }
        entry.gpuIn = bgfx::createVertexBuffer(
            bgfx::copy(data.data(),
                       static_cast<uint32_t>(data.size() * sizeof(float))),
            m_skinLayout, BGFX_BUFFER_COMPUTE_READ);
        entry.gpuOut = bgfx::createDynamicVertexBuffer(
            vcount, m_layout, BGFX_BUFFER_COMPUTE_WRITE);
        if (bgfx::isValid(entry.gpuIn) && bgfx::isValid(entry.gpuOut)) {
            entry.gpuSkinReady = true;
        }
    }

    m_meshes.push_back(std::move(entry));
    return m_meshes.back().handle;
}

void SmaMeshRenderer::destroyMesh(MeshHandle handle) {
    for (auto it = m_meshes.begin(); it != m_meshes.end(); ++it) {
        if (it->handle == handle) {
            if (bgfx::isValid(it->ib)) bgfx::destroy(it->ib);
            if (bgfx::isValid(it->gpuIn)) bgfx::destroy(it->gpuIn);
            if (bgfx::isValid(it->gpuOut)) bgfx::destroy(it->gpuOut);
            m_meshes.erase(it);
            return;
        }
    }
}

void SmaMeshRenderer::updateMesh(MeshHandle handle,
                                 const std::vector<BonePose>& poses) {
    MeshEntry* entry = find(handle);
    if (!entry || !entry->gpuReady) return;
    if (useGpuSkin(*entry)) {
        // Defer the GPU work to drawMesh (same-frame order: the bone
        // buffer upload + dispatch must run in the draw's view, before
        // its submit). Store the poses for packing at draw time.
        entry->pendingPoses = poses;
        entry->gpuDirty = true;
        return;
    }
    skinMesh(entry->mesh, poses, entry->skinned);
}

void SmaMeshRenderer::skinOnGpu(MeshEntry& entry,
                                const std::vector<BonePose>& poses,
                                uint16_t targetView,
                                float x, float y, float scale,
                                float viewW, float viewH) {
    // Pack world poses (identity rows for missing bones) + the draw
    // transform and view size into the shared bone buffer (slots 64/65).
    // The compute shader outputs FINAL NDC positions, so no per-draw
    // vertex shader uniforms are needed.
    std::vector<float> packed;
    packBonePoses(poses, kMaxBones, packed);
    const uint32_t kDrawSlot = 64;
    const uint32_t kViewSlot = 65;
    if (packed.size() < (kViewSlot + 1) * 4) {
        packed.resize((kViewSlot + 1) * 4, 0.f);
    }
    packed[kDrawSlot * 4 + 0] = x;
    packed[kDrawSlot * 4 + 1] = y;
    packed[kDrawSlot * 4 + 2] = scale;
    packed[kDrawSlot * 4 + 3] = 0.f;
    packed[kViewSlot * 4 + 0] = viewW;
    packed[kViewSlot * 4 + 1] = viewH;
    packed[kViewSlot * 4 + 2] = 0.f;
    packed[kViewSlot * 4 + 3] = 0.f;
    // bgfx::copy: the update command is executed at bgfx::frame() — the
    // referenced memory must outlive this call, so hand bgfx an owned
    // copy instead of a ref to the local vector.
    bgfx::update(m_boneBuffer, 0,
        bgfx::copy(packed.data(),
                   static_cast<uint32_t>(packed.size() * sizeof(float))));

    const uint32_t vcount = static_cast<uint32_t>(entry.mesh.vertices.size());
    const uint32_t numGroups = (vcount + 63) / 64;
    bgfx::setBuffer(0, entry.gpuIn, bgfx::Access::Read);
    bgfx::setBuffer(1, m_boneBuffer, bgfx::Access::Read);
    bgfx::setBuffer(2, entry.gpuOut, bgfx::Access::Write);
    bgfx::dispatch(targetView, m_skinProgram, numGroups, 1, 1);
    entry.gpuSkinned = true;
    entry.gpuDirty = false;
}

void SmaMeshRenderer::drawMesh(uint16_t targetView, MeshHandle handle,
                               uint32_t dstTexId, float x, float y,
                               float scale, float opacity) {
    if (!m_initialized) init();
    if (!m_initialized) return;
    MeshEntry* entry = find(handle);
    if (!entry || !entry->gpuReady) return;

    const bgfx::TextureHandle tex = { static_cast<uint16_t>(dstTexId) };
    if (!bgfx::isValid(tex)) return;

    const uint32_t idxCount = static_cast<uint32_t>(entry->mesh.indices.size());

    const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                BGFX_STATE_BLEND_INV_SRC_ALPHA);

    if (useGpuSkin(*entry)) {
        // Skin + NDC transform on the GPU (dispatch in THIS view, before
        // the draw submit that consumes the output buffer).
        const bgfx::Stats* stats = bgfx::getStats();
        const float sw = stats ? static_cast<float>(stats->width) : 1280.f;
        const float sh = stats ? static_cast<float>(stats->height) : 720.f;
        if (sw <= 0.f || sh <= 0.f) return;
        if (!entry->gpuSkinned || entry->gpuDirty) {
            skinOnGpu(*entry, entry->gpuDirty ? entry->pendingPoses
                                              : std::vector<BonePose>{},
                      targetView, x, y, scale, sw, sh);
        }

        const uint32_t vertCount =
            static_cast<uint32_t>(entry->mesh.vertices.size());
        bgfx::TransientIndexBuffer tib;
        bgfx::allocTransientIndexBuffer(&tib, idxCount);
        std::memcpy(tib.data, entry->mesh.indices.data(),
                    idxCount * sizeof(uint16_t));

        bgfx::setVertexBuffer(0, entry->gpuOut, 0, vertCount);
        bgfx::setIndexBuffer(&tib);
        bgfx::setState(state);
        bgfx::setTexture(0, m_shaders->getDefaultSampler(), tex);
        bgfx::submit(targetView, m_shaders->getFallbackProgram());
        return;
    }

    // CPU path (S2): transient VB with the pixel->NDC transform applied.
    const uint32_t vertCount = static_cast<uint32_t>(entry->skinned.size());
    if (bgfx::getAvailTransientVertexBuffer(vertCount, m_layout) < vertCount) return;
    if (bgfx::getAvailTransientIndexBuffer(idxCount) < idxCount) return;

    const bgfx::Stats* stats = bgfx::getStats();
    const float sw = stats ? static_cast<float>(stats->width) : 1280.f;
    const float sh = stats ? static_cast<float>(stats->height) : 720.f;
    if (sw <= 0.f || sh <= 0.f) return;

    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, vertCount, m_layout);
    auto* verts = reinterpret_cast<SmaSkinnedVertex*>(tvb.data);
    for (uint32_t i = 0; i < vertCount; ++i) {
        const float px = x + entry->skinned[i].x * scale;
        const float py = y + entry->skinned[i].y * scale;
        verts[i].x = (px / sw) * 2.0f - 1.0f;
        verts[i].y = 1.0f - (py / sh) * 2.0f;
        verts[i].u = entry->skinned[i].u;
        verts[i].v = entry->skinned[i].v;
    }

    bgfx::TransientIndexBuffer tib;
    bgfx::allocTransientIndexBuffer(&tib, idxCount);
    std::memcpy(tib.data, entry->mesh.indices.data(),
                idxCount * sizeof(uint16_t));

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setState(state);
    bgfx::setTexture(0, m_shaders->getDefaultSampler(), tex);
    float bp[8] = { opacity, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    bgfx::setUniform(m_shaders->getBlendParams(), bp, 2);
    bgfx::submit(targetView, m_shaders->getFallbackProgram());
}

} // namespace Caesura
