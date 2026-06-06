#pragma once

#include <bgfx/bgfx.h>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace Caesura {

// ---------------------------------------------------------------------------
// DirtyRect — sub-region of the framebuffer that needs redrawing
// ---------------------------------------------------------------------------

struct DirtyRect {
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;

    bool empty() const { return w == 0 || h == 0; }
    uint32_t area() const { return static_cast<uint32_t>(w) * static_cast<uint32_t>(h); }

    // Merge another rect into this one (union)
    void merge(const DirtyRect& o) {
        if (o.empty()) return;
        if (empty()) {
            *this = o;
            return;
        }
        uint16_t nx = std::min(x, o.x);
        uint16_t ny = std::min(y, o.y);
        uint16_t nx2 = std::max<uint16_t>(x + w, o.x + o.w);
        uint16_t ny2 = std::max<uint16_t>(y + h, o.y + o.h);
        x = nx;
        y = ny;
        w = nx2 - nx;
        h = ny2 - ny;
    }
};

// ---------------------------------------------------------------------------
// Layer -- state for a single compositing layer in the KAG visual pipeline
// ---------------------------------------------------------------------------

struct Layer {
    bgfx::TextureHandle tex     = BGFX_INVALID_HANDLE;
    float x    = 0.0f;
    float y    = 0.0f;
    float sx   = 1.0f;    // scale X
    float sy   = 1.0f;    // scale Y
    float opacity = 1.0f;
    bool  visible = false;
    bool  dirty   = true;  // needs re-render
    int   blend   = 0;     // blend mode index
};

// ---------------------------------------------------------------------------
// LayerManager — simple state wrapper for bg / fg / message layers
// ---------------------------------------------------------------------------
// Manages the three Z-ordered layers of the KAG rendering pipeline:
//   BG  (index 0) — background / scene art
//   FG  (index 1) — foreground / character sprites
//   MSG (index 2) — message box / UI overlay
//
// Does NOT own the bgfx textures; the caller (RenderBinding / Lua layer)
// is responsible for loading and passing texture handles.
//
// Registered in BackendRegistry for access from both C++ and Lua.
//
// Dirty rect: tracks changed screen regions each frame to issue scissor
// rects via bgfx::setScissor(), avoiding full-frame redraw when only a
// small portion changes.  If the union of dirty rects exceeds 75% of the
// full frame, the scissor is dropped and the entire frame is drawn.

class LayerManager {
public:
    enum LayerType : uint8_t {
        BG  = 0,
        FG  = 1,
        MSG = 2,
        COUNT = 3
    };

    static LayerManager& instance();

    LayerManager(const LayerManager&) = delete;
    LayerManager& operator=(const LayerManager&) = delete;

    void init();
    void shutdown();

    // -- Per-layer access --------------------------------------------------
    Layer& get(LayerType t);
    const Layer& get(LayerType t) const;

    // -- Convenience setters -----------------------------------------------
    void setTexture(LayerType t, bgfx::TextureHandle tex);
    void setVisible(LayerType t, bool visible);
    void setOpacity(LayerType t, float opacity);
    void setPosition(LayerType t, float x, float y);
    void setScale(LayerType t, float sx, float sy);
    void setBlendMode(LayerType t, int blend);

    // -- Utilities ---------------------------------------------------------
    void clear(LayerType t);
    void clearAll();
    void markAllDirty();

    // -- Dirty rect tracking -----------------------------------------------
    // Marks a screen-space region as needing redraw for the given layer.
    // Coordinates are in pixels, relative to the top-left of the viewport.
    void markDirty(LayerType t, uint16_t x, uint16_t y,
                   uint16_t w, uint16_t h);

    // Marks a layer dirty with transparency consideration. If the region
    // has partial transparency, extends the dirty rect to include what
    // might be revealed behind it.
    void markDirtyWithTransparency(LayerType t, uint16_t x, uint16_t y,
                                   uint16_t w, uint16_t h, bool hasAlpha);

    // Merge per-layer dirty rects and decide scissor vs full-frame.
    // Sets bgfx scissor if total dirty area < 75% of frame.
    // Call once per frame before rendering.
    void updateDirtyRegions(uint16_t screenW, uint16_t screenH);

    // Clear all dirty rects (call at end of frame)
    void clearDirtyRects();

    // -- Z-order submit ----------------------------------------------------
    // Submits all visible layers in BG → FG → MSG order to the given view.
    // Call once per frame after setting up textures.
    void render(uint16_t viewId, int screenW, int screenH,
                bgfx::ProgramHandle program);

private:
    LayerManager() = default;

    // -- Dirty rect helpers ------------------------------------------------
    bool shouldUseScissor(uint16_t screenW, uint16_t screenH) const;

    Layer m_layers[COUNT];
    bool  m_initialized = false;

    // Per-layer dirty region for the current frame
    DirtyRect m_dirtyRects[COUNT];

    // Merged dirty region across all layers (computed in updateDirtyRegions)
    DirtyRect m_mergedDirty;
    bool m_useScissor = false;
};

} // namespace Caesura