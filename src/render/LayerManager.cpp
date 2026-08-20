#include "LayerManager.h"
#include "di/BackendRegistry.h"
#include <cstdio>
#include <algorithm>
#include <cstring>

namespace Caesura {

// ---------------------------------------------------------------------------
// Default layout: legacy BG/FG/MSG three-slot setup.
// ---------------------------------------------------------------------------

void LayerManager::resetToDefaultLayout() {
    m_layers.clear();
    m_layers.resize(ILayerManager::COUNT);
    const char* names[ILayerManager::COUNT] = { "bg", "fg", "msg" };
    for (uint32_t i = 0; i < ILayerManager::COUNT; ++i) {
        m_layers[i].name = names[i];
        m_layers[i].z    = static_cast<float>(i);
    }
    m_dirtyRects.clear();
    m_dirtyRects.resize(ILayerManager::COUNT);
    m_mergedDirty = DirtyRect{};
    m_useScissor = false;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void LayerManager::init() {
    if (m_initialized) return;
    resetToDefaultLayout();  // idempotent: constructor already seeded defaults
    
    if (m_gpuEnabled) {
        m_texUniform = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler, 1);
        BackendRegistry::instance().registerDeviceLostListener(this);
    }
    m_initialized = true;
    printf("[LayerManager] Initialized (%u layers).\n",
           static_cast<unsigned>(m_layers.size()));
}

void LayerManager::shutdown() {
    if (!m_initialized) return;
    clearAll();
    if (m_gpuEnabled && bgfx::isValid(m_texUniform)) {
        bgfx::destroy(m_texUniform);
        m_texUniform = BGFX_INVALID_HANDLE;
    }
    if (m_gpuEnabled) {
        BackendRegistry::instance().unregisterDeviceLostListener(this);
    }
    m_initialized = false;
    m_layers.clear();
    m_dirtyRects.clear();
    printf("[LayerManager] Shutdown complete.\n");
}

// ---------------------------------------------------------------------------
// Dynamic layer configuration (v2)
// ---------------------------------------------------------------------------

bool LayerManager::configureLayers(const LayerConfig* configs, uint32_t count) {
    if (configs == nullptr || count == 0) return false;
    for (uint32_t i = 0; i < count; ++i) {
        if (configs[i].name == nullptr || configs[i].name[0] == '\0') return false;
    }
    for (uint32_t i = 0; i < count; ++i) {
        for (uint32_t j = i + 1; j < count; ++j) {
            if (std::strcmp(configs[i].name, configs[j].name) == 0) return false;
        }
    }
    // All good: rebuild the layer set in array order (render order).
    m_layers.clear();
    m_layers.resize(count);
    m_dirtyRects.clear();
    m_dirtyRects.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        m_layers[i].name = configs[i].name ? configs[i].name : "";
        m_layers[i].z    = configs[i].z;
    }
    m_mergedDirty = DirtyRect{};
    m_useScissor = false;
    return true;
}

uint32_t LayerManager::getLayerCount() const {
    return static_cast<uint32_t>(m_layers.size());
}

const char* LayerManager::getLayerName(uint32_t index) const {
    if (!validIndex(index)) return nullptr;
    return m_layers[index].name.c_str();
}

int32_t LayerManager::findLayer(const char* name) const {
    if (name == nullptr) return -1;
    for (size_t i = 0; i < m_layers.size(); ++i) {
        if (m_layers[i].name == name) return static_cast<int32_t>(i);
    }
    return -1;
}

bool LayerManager::reorderLayer(uint32_t fromIndex, uint32_t toIndex) {
    if (!validIndex(fromIndex) || !validIndex(toIndex)) return false;
    if (fromIndex == toIndex) return true;
    Layer moved = std::move(m_layers[fromIndex]);
    m_layers.erase(m_layers.begin() + fromIndex);
    m_layers.insert(m_layers.begin() + toIndex, std::move(moved));
    // Dirty rects follow their layer: move the dirty rect too.
    DirtyRect dr = m_dirtyRects[fromIndex];
    m_dirtyRects.erase(m_dirtyRects.begin() + fromIndex);
    m_dirtyRects.insert(m_dirtyRects.begin() + toIndex, dr);
    // The moved layer plus its neighbors changed on screen.
    markAllDirty();
    return true;
}

// ---------------------------------------------------------------------------
// Per-layer access
// ---------------------------------------------------------------------------

Layer& LayerManager::get(uint32_t index) {
    return m_layers[index];
}

const Layer& LayerManager::get(uint32_t index) const {
    return m_layers[index];
}

// ---------------------------------------------------------------------------
// Convenience setters
// ---------------------------------------------------------------------------

void LayerManager::setTexture(uint32_t t, uint32_t texId) {
    if (!validIndex(t)) return;
    Layer& l = get(t);
    l.tex   = { uint16_t(texId) };
    l.dirty = true;
}

void LayerManager::setVisible(uint32_t t, bool visible) {
    if (!validIndex(t)) return;
    Layer& l = get(t);
    l.visible = visible;
    l.dirty   = true;
}

void LayerManager::setOpacity(uint32_t t, float opacity) {
    if (!validIndex(t)) return;
    get(t).opacity = opacity;
}

void LayerManager::setPosition(uint32_t t, float x, float y) {
    if (!validIndex(t)) return;
    Layer& l = get(t);
    l.x = x;
    l.y = y;
}

void LayerManager::setScale(uint32_t t, float sx, float sy) {
    if (!validIndex(t)) return;
    Layer& l = get(t);
    l.sx = sx;
    l.sy = sy;
}

void LayerManager::setBlendMode(uint32_t t, int blend) {
    if (!validIndex(t)) return;
    get(t).blend = blend;
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

void LayerManager::clear(uint32_t t) {
    if (!validIndex(t)) return;
    Layer& l = get(t);
    l.tex     = BGFX_INVALID_HANDLE;
    l.visible = false;
    l.dirty   = true;
}

void LayerManager::clearAll() {
    for (size_t i = 0; i < m_layers.size(); ++i)
        clear(static_cast<uint32_t>(i));
}

void LayerManager::markAllDirty() {
    for (size_t i = 0; i < m_layers.size(); ++i)
        m_layers[i].dirty = true;
}

// ---------------------------------------------------------------------------
// Dirty rect tracking
// ---------------------------------------------------------------------------

void LayerManager::markDirty(uint32_t t, uint16_t x, uint16_t y,
                              uint16_t w, uint16_t h) {
    if (!validIndex(t)) return;
    DirtyRect r{x, y, w, h};
    if (r.empty()) return;
    m_dirtyRects[t].merge(r);
    m_layers[t].dirty = true;  // a dirty region implies the layer changed
}

void LayerManager::markDirtyWithTransparency(uint32_t t, uint16_t x, uint16_t y,
                                              uint16_t w, uint16_t h) {
    if (!validIndex(t)) return;

    // Mark the layer itself
    markDirty(t, x, y, w, h);

    // Recursively mark layers BELOW in render order (earlier array position =
    // lower z = rendered first) because transparency reveals what is
    // underneath. Render order may have been reordered at runtime (v2), so we
    // walk every layer that currently renders before this one.
    const uint32_t pos = t;  // array position == render position
    for (uint32_t i = 0; i < pos; ++i) {
        if (!validIndex(i)) break;
        if (!m_layers[i].visible) continue;
        markDirty(i, x, y, w, h);
    }
}

bool LayerManager::shouldUseScissor(uint16_t screenW, uint16_t screenH) const {
    return shouldUseScissorFor(m_mergedDirty, screenW, screenH);
}

bool LayerManager::shouldUseScissorFor(const DirtyRect& merged,
                                       uint16_t screenW, uint16_t screenH) {
    if (merged.empty()) return false;
    const uint32_t frameArea = static_cast<uint32_t>(screenW) * static_cast<uint32_t>(screenH);
    // Fallback: if dirty area > 75% of frame, draw full frame instead
    return merged.area() <= ((frameArea * 3u) / 4u);
}

void LayerManager::updateDirtyRegions(uint16_t screenW, uint16_t screenH) {
    // Merge all per-layer dirty rects
    m_mergedDirty = DirtyRect{};
    for (size_t i = 0; i < m_dirtyRects.size(); ++i) {
        if (!m_dirtyRects[i].empty()) {
            m_mergedDirty.merge(m_dirtyRects[i]);
        }
    }

    m_useScissor = shouldUseScissor(screenW, screenH);

    if (m_gpuEnabled && m_useScissor) {
        // bgfx scissor uses absolute pixel coords from top-left
        bgfx::setScissor(m_mergedDirty.x, m_mergedDirty.y,
                         m_mergedDirty.w, m_mergedDirty.h);
    } else if (m_gpuEnabled) {
        // Full frame: explicitly clear the scissor -- bgfx keeps the state
        // per frame, so a layer that scissored earlier would otherwise
        // clip every later full-frame submit.
        bgfx::setScissor();
    }
}

void LayerManager::clearDirtyRects() {
    for (size_t i = 0; i < m_dirtyRects.size(); ++i) {
        m_dirtyRects[i] = DirtyRect{};
    }
    m_mergedDirty = DirtyRect{};
    m_useScissor = false;
}

// ---------------------------------------------------------------------------
// Z-order submit -- array order = render order (v2; default bg -> fg -> msg)
// ---------------------------------------------------------------------------

void LayerManager::render(uint16_t viewId, int screenW, int screenH,
                           uint32_t programId) {
    if (!m_initialized || !m_gpuEnabled) return;
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

    for (size_t i = 0; i < m_layers.size(); ++i) {
        Layer& l = m_layers[i];

        if (!l.visible) continue;
        if (!bgfx::isValid(l.tex)) continue;

        // Build NDC quad covering the entire viewport with layer opacity
        float lx = l.x;
        float ly = l.y;
        float rw = (float)screenW * l.sx;
        float rh = (float)screenH * l.sy;

        // Convert to NDC: screen coordinates -> [-1, 1]
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

        bgfx::setTexture(0, m_texUniform, l.tex);
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

// ---------------------------------------------------------------------------
// IDeviceLostListener
// ---------------------------------------------------------------------------

void LayerManager::onDeviceLost() {
    if (!m_gpuEnabled) return;
    // Layer textures become invalid -- clear all layer texture references
    for (size_t i = 0; i < m_layers.size(); ++i) {
        m_layers[i].tex = BGFX_INVALID_HANDLE;
        m_layers[i].dirty = true;
    }
    // Destroy and invalidate the uniform
    if (bgfx::isValid(m_texUniform)) {
        bgfx::destroy(m_texUniform);
        m_texUniform = BGFX_INVALID_HANDLE;
    }
    printf("[LayerManager] Device lost -- layer textures released\n");
}

void LayerManager::onDeviceRestored() {
    if (!m_gpuEnabled) return;
    // Recreate the uniform
    m_texUniform = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler, 1);
    // Layers will get new textures when Lua re-calls setTexture()
    printf("[LayerManager] Device restored -- uniform recreated\n");
}

} // namespace Caesura
