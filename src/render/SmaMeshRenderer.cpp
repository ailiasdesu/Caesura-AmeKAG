#include "SmaMeshRenderer.h"
#include "BgfxShaderManager.h"
#include "SmaSkinner.h"
#include <bgfx/bgfx.h>
#include <cstring>

namespace Caesura {

// ---------------------------------------------------------------------------
// init / helpers
// ---------------------------------------------------------------------------

SmaMeshRenderer::SmaMeshRenderer() = default;

SmaMeshRenderer::~SmaMeshRenderer() = default;

void SmaMeshRenderer::init() {
    if (m_initialized) return;
    // bgfx not up (headless / CI): stay inert; every op becomes a no-op.
    if (bgfx::getRendererType() == bgfx::RendererType::Noop) return;

    m_layout
        .begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    m_shaders = std::make_unique<BgfxShaderManager>();
    m_shaders->initEmbeddedShaders();
    m_initialized = true;
}

SmaMeshRenderer::MeshEntry* SmaMeshRenderer::find(MeshHandle handle) {
    for (auto& entry : m_meshes) {
        if (entry.handle == handle) return &entry;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// IMeshRenderer
// ---------------------------------------------------------------------------

MeshHandle SmaMeshRenderer::createMesh(const SMAMesh& mesh) {
    if (!m_initialized) init();
    if (!m_initialized) return {}; // deferred-gpu: no GPU -> invalid handle
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

    m_meshes.push_back(std::move(entry));
    return m_meshes.back().handle;
}

void SmaMeshRenderer::destroyMesh(MeshHandle handle) {
    for (auto it = m_meshes.begin(); it != m_meshes.end(); ++it) {
        if (it->handle == handle) {
            if (bgfx::isValid(it->ib)) bgfx::destroy(it->ib);
            m_meshes.erase(it);
            return;
        }
    }
}

void SmaMeshRenderer::updateMesh(MeshHandle handle,
                                 const std::vector<BonePose>& poses) {
    MeshEntry* entry = find(handle);
    if (!entry || !entry->gpuReady) return;
    skinMesh(entry->mesh, poses, entry->skinned);
}

void SmaMeshRenderer::drawMesh(uint16_t targetView, MeshHandle handle,
                               uint32_t dstTexId, float x, float y,
                               float scale, float opacity) {
    if (!m_initialized) init();
    if (!m_initialized) return;
    MeshEntry* entry = find(handle);
    if (!entry || !entry->gpuReady || entry->skinned.empty()) return;

    const bgfx::TextureHandle tex = { static_cast<uint16_t>(dstTexId) };
    if (!bgfx::isValid(tex)) return;

    const uint32_t vertCount = static_cast<uint32_t>(entry->skinned.size());
    const uint32_t idxCount = static_cast<uint32_t>(entry->mesh.indices.size());
    if (bgfx::getAvailTransientVertexBuffer(vertCount, m_layout) < vertCount) return;
    if (bgfx::getAvailTransientIndexBuffer(idxCount) < idxCount) return;

    // Pixel -> NDC on the CPU (the embedded vertex shader is passthrough;
    // same convention as BgfxQuadBatch).
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

    const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                BGFX_STATE_BLEND_INV_SRC_ALPHA);
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setState(state);
    bgfx::setTexture(0, m_shaders->getDefaultSampler(), tex);
    float bp[8] = { opacity, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    bgfx::setUniform(m_shaders->getBlendParams(), bp, 2);
    bgfx::submit(targetView, m_shaders->getFallbackProgram());
}

} // namespace Caesura
