#include "LayerManager.h"
#include <cstdio>
#include <algorithm>

namespace Caesura {

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

LayerManager& LayerManager::instance() {
    static LayerManager mgr;
    return mgr;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void LayerManager::init() {
    if (m_initialized) return;
    for (int i = 0; i < COUNT; ++i) {
        m_layers[i] = Layer{};
        m_dirtyRects[i] = DirtyRect{};
    }
    m_initialized = true;
    printf("[LayerManager] Initialized.\n");
}

void LayerManager::shutdown() {
    if (!m_initialized) return;
    clearAll();
    m_initialized = false;
    printf("[LayerManager] Shutdown complete.\n");
}

// ---------------------------------------------------------------------------
// Per-layer access
// ---------------------------------------------------------------------------

Layer& LayerManager::get(LayerType t) {
    return m_layers[static_cast<uint8_t>(t)];
}

const Layer& LayerManager::get(LayerType t) const {
    return m_layers[static_cast<uint8_t>(t)];
}

// ---------------------------------------------------------------------------
// Convenience setters
// ---------------------------------------------------------------------------

void LayerManager::setTexture(LayerType t, uint32_t texId) {
    Layer& l = get(t);
    l.tex   = { uint16_t(texId) };
    l.dirty = true;
}

void LayerManager::setVisible(LayerType t, bool visible) {
    Layer& l = get(t);
    l.visible = visible;
    l.dirty   = true;
}

void LayerManager::setOpacity(LayerType t, float opacity) {
    get(t).opacity = opacity;
}

void LayerManager::setPosition(LayerType t, float x, float y) {
    Layer& l = get(t);
    l.x = x;
    l.y = y;
}

void LayerManager::setScale(LayerType t, float sx, float sy) {
    Layer& l = get(t);
    l.sx = sx;
    l.sy = sy;
}

void LayerManager::setBlendMode(LayerType t, int blend) {
    get(t).blend = blend;
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

void LayerManager::clear(LayerType t) {
    Layer& l = get(t);
    l.tex     = BGFX_INVALID_HANDLE;
    l.visible = false;
    l.dirty   = true;
}

void LayerManager::clearAll() {
    for (int i = 0; i < COUNT; ++i)
        clear(static_cast<LayerType>(i));
}

void LayerManager::markAllDirty() {
    for (int i = 0; i < COUNT; ++i)
        m_layers[i].dirty = true;
}

// ---------------------------------------------------------------------------
// Dirty rect tracking
// ---------------------------------------------------------------------------

void LayerManager::markDirty(LayerType t, uint16_t x, uint16_t y,
                              uint16_t w, uint16_t h) {
    DirtyRect r{x, y, w, h};
    if (r.empty()) return;
    m_dirtyRects[static_cast<uint8_t>(t)].merge(r);
}

void LayerManager::markDirtyWithTransparency(LayerType t, uint16_t x, uint16_t y,
                                              uint16_t w, uint16_t h) {
    int idx = static_cast<int>(t);
    if (idx >= (int)COUNT) return;

    // Mark the layer itself
    markDirty(t, x, y, w, h);

    // Recursively mark layers BELOW (lower Z-order = smaller index)
    // because transparency reveals what's underneath
    for (int i = idx - 1; i >= 0; --i) {
        markDirty(static_cast<LayerType>(i), x, y, w, h);
    }
}

bool LayerManager::shouldUseScissor(uint16_t screenW, uint16_t screenH) const {
    if (m_mergedDirty.empty()) return false;
    uint32_t frameArea = static_cast<uint32_t>(screenW) * static_cast<uint32_t>(screenH);
    // Fallback: if dirty area > 75% of frame, draw full frame instead
    return m_mergedDirty.area() <= (frameArea * 3u / 4u);
}

void LayerManager::updateDirtyRegions(uint16_t screenW, uint16_t screenH) {
    // Merge all per-layer dirty rects
    m_mergedDirty = DirtyRect{};
    for (int i = 0; i < COUNT; ++i) {
        if (!m_dirtyRects[i].empty()) {
            m_mergedDirty.merge(m_dirtyRects[i]);
        }
    }

    m_useScissor = shouldUseScissor(screenW, screenH);

    if (m_useScissor) {
        // bgfx scissor uses absolute pixel coords from top-left
        bgfx::setScissor(m_mergedDirty.x, m_mergedDirty.y,
                         m_mergedDirty.w, m_mergedDirty.h);
    }
    // else: full frame -- no scissor set (or clear scissor)
}

void LayerManager::clearDirtyRects() {
    for (int i = 0; i < COUNT; ++i) {
        m_dirtyRects[i] = DirtyRect{};
    }
    m_mergedDirty = DirtyRect{};
    m_useScissor = false;
}

// ---------------------------------------------------------------------------
// Z-order submit -- BG ¡ú FG ¡ú MSG
// ---------------------------------------------------------------------------

void LayerManager::render(uint16_t viewId, int screenW, int screenH,
                           uint32_t programId) {
    static bgfx::UniformHandle s_texUniform = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
    if (!m_initialized) return;
    if (programId == 0) return;

    // Update dirty regions for scissor optimization
    updateDirtyRegions(static_cast<uint16_t>(screenW),
                       static_cast<uint16_t>(screenH));

    // NDC quad vertices: position (2f) + texcoord (2f)
    struct PosTexVertex {
        float x, y;
        float u, v;
    };

    static bgfx::VertexLayout s_layout;
    static bool s_layoutInit = false;
    if (!s_layoutInit) {
        s_layout.begin()
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
        s_layoutInit = true;
    }

    for (int i = 0; i < COUNT; ++i) {
        Layer& l = m_layers[i];

        if (!l.visible) continue;
        if (!bgfx::isValid(l.tex)) continue;

        // Build NDC quad covering the entire viewport with layer opacity
        float lx = l.x;
        float ly = l.y;
        float rw = (float)screenW * l.sx;
        float rh = (float)screenH * l.sy;

        // Convert to NDC: screen coordinates ¡ú [-1, 1]
        float nx0 = (lx / (screenW * 0.5f)) - 1.0f;
        float ny0 = 1.0f - (ly / (screenH * 0.5f));  // flip Y
        float nx1 = ((lx + rw) / (screenW * 0.5f)) - 1.0f;
        float ny1 = 1.0f - ((ly + rh) / (screenH * 0.5f));

        if (bgfx::getAvailTransientVertexBuffer(6, s_layout) < 6)
            continue;
        if (bgfx::getAvailTransientIndexBuffer(6, false) < 6)
            continue;

        bgfx::TransientVertexBuffer tvb;
        bgfx::allocTransientVertexBuffer(&tvb, 6, s_layout);
        PosTexVertex* verts = reinterpret_cast<PosTexVertex*>(tvb.data);
        verts[0] = { nx0, ny0, 0.0f, 0.0f };
        verts[1] = { nx1, ny0, 1.0f, 0.0f };
        verts[2] = { nx1, ny1, 1.0f, 1.0f };
        verts[3] = { nx0, ny0, 0.0f, 0.0f };
        verts[4] = { nx1, ny1, 1.0f, 1.0f };
        verts[5] = { nx0, ny1, 0.0f, 1.0f };

        bgfx::TransientIndexBuffer tib;
        bgfx::allocTransientIndexBuffer(&tib, 6, false);
        uint16_t* idx = reinterpret_cast<uint16_t*>(tib.data);
        idx[0] = 0; idx[1] = 1; idx[2] = 2;
        idx[3] = 0; idx[4] = 2; idx[5] = 3;

        bgfx::setTexture(0, s_texUniform, l.tex);
        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setIndexBuffer(&tib);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                       BGFX_STATE_BLEND_ALPHA);
        bgfx::submit(viewId, bgfx::ProgramHandle{ uint16_t(programId) });

        l.dirty = false;
    }

    // Clear dirty rects after frame submission
    clearDirtyRects();
}

} // namespace Caesura