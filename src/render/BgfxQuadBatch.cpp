#include "BgfxQuadBatch.h"
#include "BgfxDraw.h"
#include "BgfxShaderManager.h"
#include "BgfxDeviceCore.h"  // getWidth/getHeight for pixel->NDC conversion
#include <bgfx/bgfx.h>
#include <cstring>

namespace Caesura {

// ===========================================================================
// Pure batch math (GPU-free) -- extracted so unit tests can pin the NDC
// conversion, merge grouping and index generation without a GPU (P2-10).
// ===========================================================================

BgfxQuadBatch::NdcRect BgfxQuadBatch::quadToNdc(float x, float y, float w, float h,
                                                float screenW, float screenH) {
    NdcRect r;
    r.nx0 = (x / screenW) * 2.0f - 1.0f;
    r.ny0 = 1.0f - (y / screenH) * 2.0f;
    r.nx1 = ((x + w) / screenW) * 2.0f - 1.0f;
    r.ny1 = 1.0f - ((y + h) / screenH) * 2.0f;
    return r;
}

void BgfxQuadBatch::computeMergeGroups(const std::vector<BatchQuad>& quads,
                                       std::vector<MergeGroup>& groups) {
    groups.clear();
    const uint32_t quadCount = (uint32_t)quads.size();
    uint32_t qi = 0;
    while (qi < quadCount) {
        const auto& q = quads[qi];
        uint32_t mergeEnd = qi + 1;
        while (mergeEnd < quadCount &&
               quads[mergeEnd].tex.idx == q.tex.idx &&
               quads[mergeEnd].viewId == q.viewId &&
               quads[mergeEnd].opacity == q.opacity) {
            mergeEnd++;
        }
        groups.push_back(MergeGroup{qi, mergeEnd - qi});
        qi = mergeEnd;
    }
}

void BgfxQuadBatch::buildGroupIndices(uint32_t quadCount,
                                      std::vector<uint16_t>& indices) {
    indices.clear();
    indices.reserve(quadCount * 6);
    for (uint32_t m = 0; m < quadCount; m++) {
        const uint32_t localBase = m * 4;
        indices.push_back((uint16_t)(localBase + 0));
        indices.push_back((uint16_t)(localBase + 1));
        indices.push_back((uint16_t)(localBase + 2));
        indices.push_back((uint16_t)(localBase + 0));
        indices.push_back((uint16_t)(localBase + 2));
        indices.push_back((uint16_t)(localBase + 3));
    }
}

void BgfxQuadBatch::beginBatch() {
    m_state->batching = true;
    m_state->batchQuads.clear();
}

void BgfxQuadBatch::flushBatch() {
    if (!m_state->batching || m_state->batchQuads.empty()) {
        m_state->batching = false;
        return;
    }

    // Ensure vertex layout is initialized (same as blitTexture lazy init)
    if (m_state->posTexLayout.getStride() == 0) {
        m_state->posTexLayout
            .begin()
            .add(bgfx::Attrib::Position,  2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
    }

    struct FsVertex { float x, y, u, v; };
    uint32_t quadCount = (uint32_t)m_state->batchQuads.size();
    uint32_t vertCount = quadCount * 4;
    uint32_t idxCount  = quadCount * 6;

    if (bgfx::getAvailTransientVertexBuffer(vertCount, m_state->posTexLayout) < vertCount) {
        m_state->batching = false;
        return;
    }
    if (bgfx::getAvailTransientIndexBuffer(idxCount) < idxCount) {
        m_state->batching = false;
        return;
    }

    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, vertCount, m_state->posTexLayout);
    auto* v = (FsVertex*)tvb.data;

    bgfx::TransientIndexBuffer tib;
    bgfx::allocTransientIndexBuffer(&tib, idxCount);
    uint16_t* indices = (uint16_t*)tib.data;

    // Split into per-texture draw calls within the batch
    // (bgfx requires one texture set per submit)

    // Pixel -> NDC conversion: the fallback vertex shader is passthrough
    // (gl_Position = vec4(a_position, 0, 1)), so all other draw paths convert
    // on the CPU side. Without it, pixel coords (0..1280, 0..720) lie outside
    // clip space [-1,1] and the whole batch is culled -- invisible UI.
    const float sw = (float)m_state->device->getWidth();
    const float sh = (float)m_state->device->getHeight();
    if (sw <= 0.0f || sh <= 0.0f) { m_state->batching = false; return; }

    for (uint32_t qi = 0; qi < quadCount; qi++) {
        auto& q = m_state->batchQuads[qi];
        uint32_t baseVert = qi * 4;

        // Build quad vertices (NDC) via the pure helper.
        const NdcRect n = quadToNdc(q.x, q.y, q.w, q.h, sw, sh);
        v[baseVert + 0] = { n.nx0, n.ny0, 0.0f, 0.0f };
        v[baseVert + 1] = { n.nx1, n.ny0, 1.0f, 0.0f };
        v[baseVert + 2] = { n.nx1, n.ny1, 1.0f, 1.0f };
        v[baseVert + 3] = { n.nx0, n.ny1, 0.0f, 1.0f };
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

    // Pure merge grouping: contiguous quads sharing (tex, viewId, opacity)
    // collapse into one submit.
    std::vector<MergeGroup> groups;
    computeMergeGroups(m_state->batchQuads, groups);

    for (const auto& g : groups) {
        const auto& q = m_state->batchQuads[g.startQuad];
        const uint32_t mergeIdxCount = g.quadCount * 6;

        // Pure local indices (rebase-from-0 within this merge group).
        std::vector<uint16_t> groupIndices;
        buildGroupIndices(g.quadCount, groupIndices);
        memcpy((uint16_t*)tib.data + idxOffset, groupIndices.data(),
               groupIndices.size() * sizeof(uint16_t));

        bgfx::setTexture(0, m_state->shaders->getDefaultSampler(), q.tex);
        // Set opacity as a uniform. blendParams is declared Vec4 x2 (8 floats);
        // passing a 1-float address with _num=1 made bgfx read 16 bytes from a
        // 4-byte stack slot (out-of-bounds read, UB). Always pass 8 floats.
        float bp[8] = { q.opacity / 255.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 0.0f };
        bgfx::setUniform(m_state->shaders->getBlendParams(), bp, 2);

        // Submit the vertex/index subset for this texture group
        bgfx::setVertexBuffer(0, &tvb, vertOffset, g.quadCount * 4);
        bgfx::setIndexBuffer(&tib, idxOffset, mergeIdxCount);
        bgfx::submit(q.viewId, m_state->shaders->getFallbackProgram());

        idxOffset += mergeIdxCount;
        vertOffset += g.quadCount * 4;
    }

    m_state->batchQuads.clear();
    m_state->batching = false;
}
} // namespace Caesura
