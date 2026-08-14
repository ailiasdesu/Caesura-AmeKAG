#include "BgfxQuadBatch.h"
#include "BgfxDraw.h"
#include "BgfxShaderManager.h"
#include "BgfxDeviceCore.h"  // getWidth/getHeight for pixel->NDC conversion
#include <bgfx/bgfx.h>

namespace Caesura {

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

        const float nx0 = (q.x / sw) * 2.0f - 1.0f;
        const float ny0 = 1.0f - (q.y / sh) * 2.0f;
        const float nx1 = ((q.x + q.w) / sw) * 2.0f - 1.0f;
        const float ny1 = 1.0f - ((q.y + q.h) / sh) * 2.0f;

        // Build quad vertices (NDC)
        v[baseVert + 0] = { nx0, ny0, 0.0f, 0.0f };
        v[baseVert + 1] = { nx1, ny0, 1.0f, 0.0f };
        v[baseVert + 2] = { nx1, ny1, 1.0f, 1.0f };
        v[baseVert + 3] = { nx0, ny1, 0.0f, 1.0f };
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
        auto& q = m_state->batchQuads[qi];

        // Check if next quad shares same texture —merge if so. Opacity is
        // part of the batch uniform (blendParams), so quads with different
        // opacity must NOT merge -- the merged submit would apply only the
        // first quad's alpha to all of them.
        uint32_t mergeEnd = qi + 1;
        while (mergeEnd < quadCount &&
               m_state->batchQuads[mergeEnd].tex.idx == q.tex.idx &&
               m_state->batchQuads[mergeEnd].viewId == q.viewId &&
               m_state->batchQuads[mergeEnd].opacity == q.opacity) {
            mergeEnd++;
        }

        uint32_t mergeCount = mergeEnd - qi;
        uint32_t mergeIdxCount = mergeCount * 6;

        bgfx::setTexture(0, m_state->shaders->getDefaultSampler(), q.tex);
        // Set opacity as a uniform. blendParams is declared Vec4 x2 (8 floats);
        // passing a 1-float address with _num=1 made bgfx read 16 bytes from a
        // 4-byte stack slot (out-of-bounds read, UB). Always pass 8 floats.
        float bp[8] = { q.opacity / 255.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 0.0f };
        bgfx::setUniform(m_state->shaders->getBlendParams(), bp, 2);

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
        bgfx::submit(q.viewId, m_state->shaders->getFallbackProgram());

        idxOffset += mergeIdxCount;
        vertOffset += mergeCount * 4;
        qi = mergeEnd - 1;
    }

    m_state->batchQuads.clear();
    m_state->batching = false;
}
} // namespace Caesura
