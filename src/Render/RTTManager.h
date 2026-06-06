#pragma once
#include "IRenderDevice.h"
#include <cstdint>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <cassert>

namespace Caesura {

// -- RTT Manager -----------------------------------------------------------
// Manages offscreen render-to-texture canvas allocation and lifecycle.
// Delegates actual GPU work to IRenderDevice.

// -- RTT type classification ------------------------------------------------
enum class RTType { RT_2D = 0, RT_3D = 1 };

// -- Pooled RTT entry -------------------------------------------------------
struct RTTEntry {
    ViewportHandle handle;
    int width = 0, height = 0;
    RTType type = RTType::RT_2D;
    bool inUse = false;
};

class RTTManager {
public:
    explicit RTTManager(IRenderDevice& device) : m_device(device) {}

    // -- Pool-based canvas API (preferred) -----------------------------------
    // Acquire a canvas from the pool (or create new). Defaults to clear.
    ViewportHandle acquireCanvas(int w, int h, RTType type, bool clear = true);

    // Return a canvas to the pool for future reuse.
    void releaseCanvas(ViewportHandle handle);

    // -- Legacy API (wraps acquireCanvas/releaseCanvas for backward compat) --
    ViewportHandle createCanvas(int width, int height);
    void destroyCanvas(ViewportHandle handle);
    void clearAll();

    // -- Deferred destruction ------------------------------------------------
    // Enqueue a handle for destruction at end-of-frame.
    void destroyCanvasDeferred(ViewportHandle handle);
    // Actually destroy all enqueued handles. Call once per frame at end.
    void flushDeferredDestroys();

    // -- Snapshot ------------------------------------------------------------
    bool captureSnapshot(const char* path, int w, int h);

private:
    IRenderDevice& m_device;

    // Pool storage — separate 2D and 3D pools
    std::vector<RTTEntry> m_pool2D;
    std::vector<RTTEntry> m_pool3D;

    // Legacy handle tracking (for backward compat createCanvas/destroyCanvas)
    std::unordered_map<uint32_t, size_t> m_handleToPoolIndex;
    std::unordered_set<uint32_t> m_legacyHandles;

    // Deferred destruction queue — handles destroyed at end-of-frame
    std::vector<ViewportHandle> m_deferredDestroy;

    // -- Internal helpers ----------------------------------------------------
    std::vector<RTTEntry>& poolFor(RTType type);
    // Find a free entry with matching size. Returns -1 if none.
    int findFreeRTT(int w, int h, RTType type);
    // Clear the RTT at acquire time.
    void clearRTT(const RTTEntry& entry);
};

} // namespace Caesura