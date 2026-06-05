#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../external/stb/stb_image_write.h"
#include "RTTManager.h"
#include <bgfx/bgfx.h>
#include <vector>
#include <cstdio>

namespace Caesura {

ViewportHandle RTTManager::createCanvas(int width, int height) {
    ViewportHandle h = m_device.createRenderTarget(width, height);
    if (h.id != 0) { m_handles.insert(h.id); }
    return h;
}

void RTTManager::destroyCanvas(ViewportHandle handle) {
    if (handle.id == 0) return;
    m_device.destroyRenderTarget(handle);
    m_handles.erase(handle.id);
}

void RTTManager::clearAll() {
    for (uint32_t id : m_handles) { m_device.destroyRenderTarget(ViewportHandle{id}); }
    m_handles.clear();
}

bool RTTManager::captureSnapshot(const char* path, int w, int h) {
    if (!path || w <= 0 || h <= 0) return false;
    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path);
    printf("[RTTManager] Snapshot requested: %s (%dx%d)\n", path, w, h);
    return true;
}

} // namespace Caesura