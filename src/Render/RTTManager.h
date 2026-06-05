#pragma once
#include "IRenderDevice.h"
#include <cstdint>
#include <unordered_set>

namespace Caesura {

// -- RTT Manager -----------------------------------------------------------
// Manages offscreen render-to-texture canvas allocation and lifecycle.
// Delegates actual GPU work to IRenderDevice.

class RTTManager {
public:
    explicit RTTManager(IRenderDevice& device) : m_device(device) {}

    // Allocate a new offscreen RTT canvas at the given resolution.
    // Returns a handle usable with IRenderDevice::blitViewport.
    ViewportHandle createCanvas(int width, int height);

    // Release the canvas and free GPU resources.
    void destroyCanvas(ViewportHandle handle);

    // Clear all canvases.
    void clearAll();

    bool captureSnapshot(const char* path, int w, int h);

private:
    IRenderDevice& m_device;
    std::unordered_set<uint32_t> m_handles;
};

} // namespace Caesura
