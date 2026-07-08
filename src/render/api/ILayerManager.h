#pragma once
#include <cstdint>

namespace Caesura {

// ============================================================================
// ILayerManager — pure virtual interface for layer compositing
// ============================================================================
// LayerManager implements this interface. BackendRegistry stores ILayerManager*.

class ILayerManager {
public:
    // [R11-FIX] Layer architecture note:
    // C++ ILayerManager provides 3 low-level composition slots (BG/FG/MSG).
    // This is the HARDWARE composition layer - simple Z-ordered texture blitting.
    //
    // Lua layers.lua provides a 7-type high-level scene graph on top:
    //   LAYER_BASE(1) / LAYER_LAYER0(2) / LAYER_LAYER1(3) / LAYER_FORE(4) /
    //   LAYER_UI(5) / LAYER_MESSAGE(6) / LAYER_EFFECT(7)
    //
    // The Lua layer tree composes into RTTs and submits batches via
    // Render.submit_batch() -> IRenderDevice::beginBatch()/flushBatch(),
    // bypassing the simple 3-slot compositor for production rendering.
    //
    // The 3-slot compositor is retained for simple use cases and backward compat.
    enum LayerType : uint8_t {
        BG  = 0,
        FG  = 1,
        MSG = 2,
        COUNT = 3
    };

    virtual ~ILayerManager() = default;

    virtual void init() = 0;
    virtual void shutdown() = 0;

    // texId: raw bgfx TextureHandle.idx value
    virtual void setTexture(LayerType t, uint32_t texId) = 0;
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

    // programId: raw bgfx ProgramHandle.idx value
    virtual void render(uint16_t viewId, int screenW, int screenH,
                        uint32_t programId) = 0;
};

} // namespace Caesura
