#pragma once

#include "api/ILayerManager.h"
#include "../di/api/IDeviceLostListener.h"
#include <bgfx/bgfx.h>
#include <cstdint>
#include <string>
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
        // Compute in 32-bit: x+w etc. can exceed 65535 for pathological
        // inputs; casting early to uint16_t would wrap and produce a wrong
        // (possibly inverted) merged rect. We clamp the merged extent to the
        // representable range instead -- a valid (if larger) bounding box,
        // which is all the scissor optimization needs (G8).
        const uint32_t kMax = 65535u;
        const uint32_t nx = std::min<uint32_t>(x, o.x);
        const uint32_t ny = std::min<uint32_t>(y, o.y);
        const uint32_t nx2 = std::min(kMax,
            std::max<uint32_t>(uint32_t(x) + w, uint32_t(o.x) + o.w));
        const uint32_t ny2 = std::min(kMax,
            std::max<uint32_t>(uint32_t(y) + h, uint32_t(o.y) + o.h));
        x = (uint16_t)nx; y = (uint16_t)ny;
        w = (uint16_t)(nx2 - nx); h = (uint16_t)(ny2 - ny);
    }
};

struct Layer {
    std::string name;                    // unique layer name (v2)
    float z      = 0.0f;                 // logical depth hint
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

class LayerManager : public ILayerManager, public IDeviceLostListener {
public:
    explicit LayerManager(bool gpuEnabled = false) : m_gpuEnabled(gpuEnabled) { resetToDefaultLayout(); }

    LayerManager(const LayerManager&) = delete;
    LayerManager& operator=(const LayerManager&) = delete;

    void init() override;
    void shutdown() override;

    // Accessors (not in interface -> returns internal Layer& for direct manipulation)
    Layer& get(uint32_t index);
    const Layer& get(uint32_t index) const;

    // -- Dynamic layer configuration (v2) --
    bool configureLayers(const LayerConfig* configs, uint32_t count) override;
    uint32_t getLayerCount() const override;
    const char* getLayerName(uint32_t index) const override;
    int32_t findLayer(const char* name) const override;
    bool reorderLayer(uint32_t fromIndex, uint32_t toIndex) override;

    void setTexture(uint32_t t, uint32_t texId) override;
    void setVisible(uint32_t t, bool visible) override;
    void setOpacity(uint32_t t, float opacity) override;
    void setPosition(uint32_t t, float x, float y) override;
    void setScale(uint32_t t, float sx, float sy) override;
    void setBlendMode(uint32_t t, int blend) override;

    void clear(uint32_t t) override;
    void clearAll() override;
    void markAllDirty() override;

    void markDirty(uint32_t t, uint16_t x, uint16_t y,
                   uint16_t w, uint16_t h) override;
    void markDirtyWithTransparency(uint32_t t, uint16_t x, uint16_t y,
                                   uint16_t w, uint16_t h) override;
    void updateDirtyRegions(uint16_t screenW, uint16_t screenH) override;
    void clearDirtyRects() override;

    // Pure scissor decision (GPU-free): true when the merged dirty area is
    // <= 75% of the frame -- otherwise redrawing the whole frame is cheaper.
    static bool shouldUseScissorFor(const DirtyRect& merged,
                                    uint16_t screenW, uint16_t screenH);

    void render(uint16_t viewId, int screenW, int screenH,
                uint32_t programId) override;

    // -- IDeviceLostListener --
    void onDeviceLost() override;
    void onDeviceRestored() override;

private:
    bool shouldUseScissor(uint16_t screenW, uint16_t screenH) const;

    void resetToDefaultLayout();
    bool validIndex(uint32_t index) const { return index < m_layers.size(); }

    std::vector<Layer> m_layers;         // array order = render order (v2)
    bool  m_initialized = false;
    bool  m_gpuEnabled = false;

    bgfx::UniformHandle m_texUniform = BGFX_INVALID_HANDLE;

    std::vector<DirtyRect> m_dirtyRects;
    DirtyRect m_mergedDirty;
    bool m_useScissor = false;
};

} // namespace Caesura
