#pragma once
#include <bgfx/bgfx.h>
#include <cstdint>

namespace Caesura {

// ============================================================================
// ILayerManager — pure virtual interface for layer compositing
// ============================================================================
// LayerManager implements this interface. BackendRegistry stores ILayerManager*.

class ILayerManager {
public:
    enum LayerType : uint8_t {
        BG  = 0,
        FG  = 1,
        MSG = 2,
        COUNT = 3
    };

    virtual ~ILayerManager() = default;

    virtual void init() = 0;
    virtual void shutdown() = 0;

    virtual void setTexture(LayerType t, bgfx::TextureHandle tex) = 0;
    virtual void setVisible(LayerType t, bool visible) = 0;
    virtual void setOpacity(LayerType t, float opacity) = 0;
    virtual void setPosition(LayerType t, float x, float y) = 0;
    virtual void setScale(LayerType t, float sx, float sy) = 0;
    virtual void setBlendMode(LayerType t, int blend) = 0;

    virtual void clear(LayerType t) = 0;
    virtual void clearAll() = 0;
    virtual void markAllDirty() = 0;

    virtual void markDirty(LayerType t, uint16_t x, uint16_t y,
                           uint16_t w, uint16_t h) = 0;
    virtual void markDirtyWithTransparency(LayerType t, uint16_t x, uint16_t y,
                                           uint16_t w, uint16_t h) = 0;
    virtual void updateDirtyRegions(uint16_t screenW, uint16_t screenH) = 0;
    virtual void clearDirtyRects() = 0;

    virtual void render(uint16_t viewId, int screenW, int screenH,
                        bgfx::ProgramHandle program) = 0;
};

} // namespace Caesura
