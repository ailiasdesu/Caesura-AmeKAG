#pragma once
#include <cstdint>

namespace Caesura {

// ============================================================================
// ILayerManager — pure virtual interface for layer compositing
// ============================================================================
// LayerManager implements this interface. BackendRegistry stores ILayerManager*.
//
// v2 (round 116): the hard compositor is no longer restricted to the legacy
// 3-slot BG/FG/MSG layout. Callers may configure any number of named layers
// with an explicit render order:
//
//   ILayerManager::LayerConfig cfg[] = { {"bg", 0.0f}, {"sprite_a", 10.0f},
//                                        {"fg", 100.0f} };
//   mgr->configureLayers(cfg, 3);
//   mgr->setTexture(mgr->findLayer("sprite_a"), texId);
//   mgr->render(view, w, h, program);   // renders bg -> sprite_a -> fg
//
// The legacy LayerType enum (BG=0/FG=1/MSG=2) remains as the DEFAULT layout
// indices; every index-based method accepts those values unchanged, so
// existing call sites keep compiling against the same semantics.
//
// R11 note: Lua layers.lua provides a 7-type high-level scene graph (dynamic
// count/names/z-order) that composes into RTTs and submits via
// Render.submit_batch() -> IRenderDevice::beginBatch()/flushBatch(), bypassing
// this simple compositor for production rendering. This interface is retained
// for simple use cases, backward compatibility, and as the C++-facing
// counterpart of the Lua scene graph.
class ILayerManager {
public:
    // Legacy default layout indices (backward compatible).
    enum LayerType : uint8_t {
        BG  = 0,
        FG  = 1,
        MSG = 2,
        COUNT = 3
    };

    // Runtime layer configuration. A layer's render order is its position in
    // the configured array (low-to-high z, first rendered first).
    struct LayerConfig {
        const char* name = nullptr;  // unique display/query name (required)
        float       z    = 0.0f;     // logical depth hint (informational;
                                     // actual order = array position)
    };

    virtual ~ILayerManager() = default;

    virtual void init() = 0;
    virtual void shutdown() = 0;

    // ---- Dynamic layer configuration (v2) ---------------------------------
    // Reconfigure the layer set. Replaces every existing layer: all texture
    // references and dirty state are cleared; callers must re-apply textures.
    // Fails (false) on a null/duplicate/empty name or a zero count. On
    // success the manager contains exactly `count` layers in array order.
    virtual bool configureLayers(const LayerConfig* configs, uint32_t count) = 0;

    // Current number of configured layers (>= 1 after init; 0 before init).
    virtual uint32_t getLayerCount() const = 0;

    // Name of the layer at `index` (nullptr when index is out of range).
    virtual const char* getLayerName(uint32_t index) const = 0;

    // Resolve a layer name to its index, or -1 when unknown. Names are
    // case-sensitive.
    virtual int32_t findLayer(const char* name) const = 0;

    // Reorder layers at runtime: move the layer currently at fromIndex so it
    // renders at toIndex (both in [0, getLayerCount())). Succeeds (true) only
    // for in-range indices; other layers shift to keep the set contiguous.
    virtual bool reorderLayer(uint32_t fromIndex, uint32_t toIndex) = 0;

    // ---- Per-layer state (index-based; LayerType values are the default
    //      layout indices) ---------------------------------------------------

    // texId: raw bgfx TextureHandle.idx value
    virtual void setTexture(uint32_t t, uint32_t texId) = 0;
    virtual void setVisible(uint32_t t, bool visible) = 0;
    virtual void setOpacity(uint32_t t, float opacity) = 0;
    virtual void setPosition(uint32_t t, float x, float y) = 0;
    virtual void setScale(uint32_t t, float sx, float sy) = 0;
    virtual void setBlendMode(uint32_t t, int blend) = 0;

    virtual void clear(uint32_t t) = 0;
    virtual void clearAll() = 0;
    virtual void markAllDirty() = 0;

    virtual void markDirty(uint32_t t, uint16_t x, uint16_t y,
                           uint16_t w, uint16_t h) = 0;
    virtual void markDirtyWithTransparency(uint32_t t, uint16_t x, uint16_t y,
                                           uint16_t w, uint16_t h) = 0;
    virtual void updateDirtyRegions(uint16_t screenW, uint16_t screenH) = 0;
    virtual void clearDirtyRects() = 0;

    // programId: raw bgfx ProgramHandle.idx value
    virtual void render(uint16_t viewId, int screenW, int screenH,
                        uint32_t programId) = 0;
};

} // namespace Caesura
