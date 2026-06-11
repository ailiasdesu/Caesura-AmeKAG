#include "BgfxDraw.h"
#include "BgfxShaderManager.h"
#include "BgfxDeviceCore.h"
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <cstdio>

namespace Caesura {

void BgfxDraw::init(DrawState* state) { m_state = state; }

void BgfxDraw::beginBatch() {
    m_state->batching = true;
    m_state->batchQuads.clear();
}

void BgfxDraw::flushBatch() {
    if (!m_state->batching || m_state->batchQuads.empty()) {
        m_state->batching = false;
        return;
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

    uint16_t currentView = 0;
    uint32_t quadInBatch = 0;

    // Split into per-texture draw calls within the batch
    // (bgfx requires one texture set per submit)
    uint32_t baseVert = 0;
    uint32_t baseIdx  = 0;

    for (uint32_t qi = 0; qi < quadCount; qi++) {
        auto& q = m_state->batchQuads[qi];

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
        auto& q = m_state->batchQuads[qi];

        // Check if next quad shares same texture ??merge if so
        uint32_t mergeEnd = qi + 1;
        while (mergeEnd < quadCount &&
               m_state->batchQuads[mergeEnd].tex.idx == q.tex.idx &&
               m_state->batchQuads[mergeEnd].viewId == q.viewId) {
            mergeEnd++;
        }

        uint32_t mergeCount = mergeEnd - qi;
        uint32_t mergeIdxCount = mergeCount * 6;

        bgfx::setTexture(0, m_state->shaders->getDefaultSampler(), q.tex);
        // Set opacity as a uniform if needed
        float alpha = q.opacity / 255.0f;
        bgfx::setUniform(m_state->shaders->getBlendParams(), &alpha, 1);

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

void BgfxDraw::stretchBlt(uint16_t targetView, uint32_t dstTexId,
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

    float hw = (float)m_state->device->getWidth()  * 0.5f;
    float hh = (float)m_state->device->getHeight() * 0.5f;
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
    bgfx::setUniform(m_state->shaders->getStretchParams(), stretchData, 1);

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setState(state);
    bgfx::setTexture(0, m_state->shaders->getDefaultSampler(), srcTex);

    bgfx::ProgramHandle prog = bgfx::isValid(m_state->shaders->getStretchProgram()) ? m_state->shaders->getStretchProgram() : m_state->shaders->getFallbackProgram();
    bgfx::submit(targetView, prog);
}

void BgfxDraw::affineBlt(uint16_t targetView, uint32_t dstTexId,
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

    float hw = (float)m_state->device->getWidth()  * 0.5f;
    float hh = (float)m_state->device->getHeight() * 0.5f;
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
    bgfx::setUniform(m_state->shaders->getAffineParams(), affineData, 4);
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setState(state);
    bgfx::setTexture(0, m_state->shaders->getDefaultSampler(), srcTex);

    bgfx::ProgramHandle prog = bgfx::isValid(m_state->shaders->getAffineProgram()) ? m_state->shaders->getAffineProgram() : m_state->shaders->getFallbackProgram();
    bgfx::submit(targetView, prog);
}

void BgfxDraw::submitBlend(uint16_t viewId, bgfx::TextureHandle baseTex,
                                    bgfx::TextureHandle blendTex, int mode,
                                    float baseAlpha, float blendAlpha, float globalAlpha) {
    if (!bgfx::isValid(m_state->shaders->getBlendProgram())) {
        static bool once = false;
        if (!once) { fprintf(stderr, "[BgfxRenderDevice] submitBlend: blend program not loaded.\n"); once = true; }
        return;
    }

    bgfx::setTexture(0, m_state->shaders->getDefaultSampler(), baseTex);
    bgfx::setTexture(1, m_state->shaders->getDefaultSampler(), blendTex);

    float params[8] = { baseAlpha, blendAlpha, globalAlpha, (float)mode, 0, 0, 0, 0 };
    bgfx::setUniform(m_state->shaders->getBlendParams(), params, 2);

    submitFullscreenQuad(viewId, m_state->shaders->getBlendProgram(), 0, 0, (float)m_state->device->getWidth(), (float)m_state->device->getHeight(), BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, nullptr, 0);
}

void BgfxDraw::submitTransition(uint16_t viewId, bgfx::TextureHandle fromTex,
                                         bgfx::TextureHandle toTex,
                                         bgfx::TextureHandle ruleTex,
                                         int method, float progress) {
    if (!bgfx::isValid(m_state->shaders->getTransitionProgram())) {
        static bool once = false;
        if (!once) { fprintf(stderr, "[BgfxRenderDevice] submitTransition: transition program not loaded.\n"); once = true; }
        return;
    }

    bgfx::setTexture(0, m_state->shaders->getDefaultSampler(), fromTex);
    bgfx::setTexture(1, m_state->shaders->getDefaultSampler(), toTex);
    if (bgfx::isValid(ruleTex))
        bgfx::setTexture(2, m_state->shaders->getDefaultSampler(), ruleTex);

    float params[4] = { progress, (float)method, 0, 0 };
    bgfx::setUniform(m_state->shaders->getTransParams(), params, 1);

    submitFullscreenQuad(viewId, m_state->shaders->getTransitionProgram(), 0, 0, (float)m_state->device->getWidth(), (float)m_state->device->getHeight(), BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, nullptr, 0);
}

void BgfxDraw::submitVFX(uint16_t viewId, bgfx::TextureHandle srcTex,
                                  int effect, float fadeAlpha,
                                  float fadeR, float fadeG, float fadeB,
                                  float blurRadius, float quakeX, float quakeY) {
    if (!bgfx::isValid(m_state->shaders->getVFXProgram())) {
        static bool once = false;
        if (!once) { fprintf(stderr, "[BgfxRenderDevice] submitVFX: VFX program not loaded.\n"); once = true; }
        return;
    }

    bgfx::setTexture(0, m_state->shaders->getDefaultSampler(), srcTex);

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
    bgfx::setUniform(m_state->shaders->getVFXParams(), vfxData, 3);

    submitFullscreenQuad(viewId, m_state->shaders->getVFXProgram(), 0, 0, (float)m_state->device->getWidth(), (float)m_state->device->getHeight(), srcTex, m_state->shaders->getDefaultSampler(), BGFX_INVALID_HANDLE, nullptr, 0);
}

void BgfxDraw::fillViewport(ViewportHandle handle,
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

    if (!bgfx::isValid(colorTex) || !bgfx::isValid(m_state->shaders->getFallbackProgram())) {
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
    bgfx::setTexture(0, m_state->shaders->getDefaultSampler(), colorTex);
    bgfx::setState(state);
    bgfx::submit(vpView, m_state->shaders->getFallbackProgram());

    bgfx::destroy(colorTex);
    bgfx::setViewFrameBuffer(vpView, it->second.fb);

    printf("[BgfxRenderDevice] fillViewport #%u: (%d,%d,%d,%d) -> view %u\n",
           handle.id, r, g, b, a, (unsigned)vpView);
}

void BgfxDraw::blitTexture(uint16_t targetView, uint32_t textureId,
                                    float x, float y, float w, float h, uint8_t opacity) {
    // Batch protocol: when batching, accumulate quads instead of drawing
    if (m_state->batching) {
        m_state->batchQuads.push_back({targetView, { static_cast<uint16_t>(textureId) }, x, y, w, h, opacity});
        return;
    }
    bgfx::TextureHandle tex = { uint16_t(textureId) };
    blitTexture(targetView, tex, x, y, w, h, opacity);
}

void BgfxDraw::blitTexture(uint16_t targetView, bgfx::TextureHandle tex,
                                    float x, float y, float w, float h, uint8_t opacity) {
    if (!bgfx::isValid(tex)) return;

    // Ortho projection for screen-space quad
    float ortho[16];
    const bgfx::Caps* caps = bgfx::getCaps();
    bx::mtxOrtho(ortho, 0.0f, float(m_width), float(m_height), 0.0f,
                 -1.0f, 1.0f, 0.0f, caps ? caps->homogeneousDepth : false,
                 bx::Handedness::Left);
    bgfx::setViewTransform(targetView, nullptr, ortho);

    // Lazy-init vertex layout
    if (m_state->posTexLayout.getStride() == 0) {
        m_state->posTexLayout
            .begin()
            .add(bgfx::Attrib::Position,  2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
    }

    // Lazy-init sampler uniform
    if (!bgfx::isValid(m_state->shaders->getDefaultSampler())) {
        m_state->shaders->getDefaultSampler() = bgfx::createUniform("s_texture", bgfx::UniformType::Sampler);
    }

    // Pixel coords -> NDC (passthrough shader bypasses u_viewProj)
    float sw = (float)m_state->device->getWidth();
    float sh = (float)m_state->device->getHeight();
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
    if (bgfx::getAvailTransientVertexBuffer(4, m_state->posTexLayout) < 4) return;
    bgfx::allocTransientVertexBuffer(&tvb, 4, m_state->posTexLayout);
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
    if (!bgfx::isValid(m_state->shaders->getFallbackProgram())) return;

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setTexture(0, m_state->shaders->getDefaultSampler(), tex);
    bgfx::setState(state);
    bgfx::submit(targetView, m_state->shaders->getFallbackProgram());
}

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

} // namespace Caesura
