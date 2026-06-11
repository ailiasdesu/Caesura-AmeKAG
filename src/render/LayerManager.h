#pragma once

#include "api/ILayerManager.h"
#include <bgfx/bgfx.h>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace Caesura {

struct DirtyRect {
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;

    bool empty() const { return w == 0 || h == 0; }
    uint32_t area() const { return static_cast<uint32_t>(w) * static_cast<uint32_t>(h); }

    void merge(const DirtyRect& o) {
        if (o.empty()) return;
        if (empty()) { *this = o; return; }
        uint16_t nx = std::min(x, o.x);
        uint16_t ny = std::min(y, o.y);
        uint16_t nx2 = std::max<uint16_t>(x + w, o.x + o.w);
        uint16_t ny2 = std::max<uint16_t>(y + h, o.y + o.h);
        x = nx; y = ny; w = nx2 - nx; h = ny2 - ny;
    }
};

struct Layer {
    bgfx::TextureHandle tex     = BGFX_INVALID_HANDLE;
    float x    = 0.0f;
    float y    = 0.0f;
    float sx   = 1.0f;
    float sy   = 1.0f;
    float opacity = 1.0f;
    bool  visible = false;
    bool  dirty   = true;
    int   blend   = 0;
};

// ============================================================================
// LayerManager -- implements ILayerManager
// ============================================================================

class LayerManager : public ILayerManager {
public:
    static LayerManager& instance();

    LayerManager(const LayerManager&) = delete;
    LayerManager& operator=(const LayerManager&) = delete;

    void init() override;
    void shutdown() override;

    // Accessors (not in interface — returns internal Layer& for direct manipulation)
    Layer& get(LayerType t);
    const Layer& get(LayerType t) const;

    void setTexture(LayerType t, bgfx::TextureHandle tex) override;
    void setVisible(LayerType t, bool visible) override;
    void setOpacity(LayerType t, float opacity) override;
    void setPosition(LayerType t, float x, float y) override;
    void setScale(LayerType t, float sx, float sy) override;
    void setBlendMode(LayerType t, int blend) override;

    void clear(LayerType t) override;
    void clearAll() override;
    void markAllDirty() override;

    void markDirty(LayerType t, uint16_t x, uint16_t y,
                   uint16_t w, uint16_t h) override;
    void markDirtyWithTransparency(LayerType t, uint16_t x, uint16_t y,
                                   uint16_t w, uint16_t h) override;
    void updateDirtyRegions(uint16_t screenW, uint16_t screenH) override;
    void clearDirtyRects() override;

    void render(uint16_t viewId, int screenW, int screenH,
                bgfx::ProgramHandle program) override;

private:
    LayerManager() = default;

    bool shouldUseScissor(uint16_t screenW, uint16_t screenH) const;

    Layer m_layers[COUNT];
    bool  m_initialized = false;

    DirtyRect m_dirtyRects[COUNT];
    DirtyRect m_mergedDirty;
    bool m_useScissor = false;
};

} // namespace Caesura