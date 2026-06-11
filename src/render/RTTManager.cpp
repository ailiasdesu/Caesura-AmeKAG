#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../external/stb/stb_image_write.h"
#include "RTTManager.h"
#include <bgfx/bgfx.h>
#include <vector>
#include <cstdio>
#include "../script/vm/LuaManager.h"

namespace Caesura {

// ===========================================================================
// Internal helpers
// ===========================================================================

std::vector<RTTEntry>& RTTManager::poolFor(RTType type) {
    return (type == RTType::RT_3D) ? m_pool3D : m_pool2D;
}

int RTTManager::findFreeRTT(int w, int h, RTType type) {
    auto& pool = poolFor(type);
    for (size_t i = 0; i < pool.size(); i++) {
        if (!pool[i].inUse && pool[i].width == w && pool[i].height == h && pool[i].type == type) {
            return (int)i;
        }
    }
    return -1;
}

void RTTManager::clearRTT(const RTTEntry& entry) {
    // Set the RTT view to clear to ensure clean state on acquire.
    // We touch the view so bgfx knows it exists, then set a clear.
    bgfx::setViewRect((uint16_t)entry.handle.id, 0, 0,
                      (uint16_t)entry.width, (uint16_t)entry.height);
    bgfx::setViewClear((uint16_t)entry.handle.id,
                       BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0x00000000, 1.0f, 0);
    bgfx::touch((uint16_t)entry.handle.id);
}

// ===========================================================================
// Pool-based API
// ===========================================================================

ViewportHandle RTTManager::acquireCanvas(int w, int h, RTType type, bool clear) {

    if (w <= 0 || h <= 0) return ViewportHandle{0};

    // Try to find a free matching RTT in the pool
    int idx = findFreeRTT(w, h, type);
    if (idx >= 0) {
        auto& pool = poolFor(type);
        pool[idx].inUse = true;
        if (clear) {
            clearRTT(pool[idx]);
        }
        printf("[RTTManager] Reused pooled %s RTT %u (%dx%d)\n",
               (type == RTType::RT_3D ? "3D" : "2D"),
               pool[idx].handle.id, w, h);
        return pool[idx].handle;
    }

    // No free match -- create a new RTT
    ViewportHandle hdl = m_device.createRenderTarget(w, h);
    if (hdl.id == 0) {
        fprintf(stderr, "[RTTManager] Failed to create %s RTT (%dx%d)\n",
                (type == RTType::RT_3D ? "3D" : "2D"), w, h);
        return hdl;
    }

    RTTEntry entry;
    entry.handle = hdl;
    entry.width  = w;
    entry.height = h;
    entry.type   = type;
    entry.inUse  = true;

    auto& pool = poolFor(type);
    pool.push_back(entry);
    m_handleToPoolIndex[hdl.id] = pool.size() - 1;

    if (clear) {
        clearRTT(entry);
    }

    printf("[RTTManager] Created new %s RTT %u (%dx%d), pool size=%zu\n",
           (type == RTType::RT_3D ? "3D" : "2D"),
           hdl.id, w, h, pool.size());
    return hdl;
}

void RTTManager::releaseCanvas(ViewportHandle handle) {

    if (handle.id == 0) return;

    auto it = m_handleToPoolIndex.find(handle.id);
    if (it != m_handleToPoolIndex.end()) {
        size_t idx = it->second;
        // Check both pools
        if (idx < m_pool2D.size() && m_pool2D[idx].handle.id == handle.id) {
            m_pool2D[idx].inUse = false;
            printf("[RTTManager] Released 2D RTT %u back to pool (%dx%d)\n",
                   handle.id, m_pool2D[idx].width, m_pool2D[idx].height);
            return;
        }
        if (idx < m_pool3D.size() && m_pool3D[idx].handle.id == handle.id) {
            m_pool3D[idx].inUse = false;
            printf("[RTTManager] Released 3D RTT %u back to pool (%dx%d)\n",
                   handle.id, m_pool3D[idx].width, m_pool3D[idx].height);
            return;
        }
    }

    // Not in pool -- fallback: just mark as available in whichever pool matches
    fprintf(stderr, "[RTTManager] releaseCanvas: handle %u not found in pool index, searching...\n", handle.id);
    for (auto& entry : m_pool2D) {
        if (entry.handle.id == handle.id) { entry.inUse = false; return; }
    }
    for (auto& entry : m_pool3D) {
        if (entry.handle.id == handle.id) { entry.inUse = false; return; }
    }
}

// ===========================================================================
// Deferred destruction
// ===========================================================================

void RTTManager::destroyCanvasDeferred(ViewportHandle handle) {

    if (handle.id == 0) return;
    m_deferredDestroy.push_back(handle);
    printf("[RTTManager] Deferred destroy for RTT %u (queue size=%zu)\n",
           handle.id, m_deferredDestroy.size());
}

void RTTManager::flushDeferredDestroys() {

    if (m_deferredDestroy.empty()) return;

    printf("[RTTManager] Flushing %zu deferred destroys...\n", m_deferredDestroy.size());

    for (auto& handle : m_deferredDestroy) {
        if (handle.id == 0) continue;

        // Remove from pool
        m_handleToPoolIndex.erase(handle.id);
        for (auto& pool : {&m_pool2D, &m_pool3D}) {
            for (auto it = pool->begin(); it != pool->end(); ++it) {
                if (it->handle.id == handle.id) {
                    m_device.destroyRenderTarget(it->handle);
                    size_t erasedIdx = std::distance(pool->begin(), it);
                    pool->erase(it);
                    // Fixup indices: after erase, all entries shifted left by 1
                    for (auto& kv : m_handleToPoolIndex) {
                        if (kv.second > erasedIdx) --kv.second;
                    }
                    goto next_handle;
                }
            }
        }

        // Legacy handles
        if (m_legacyHandles.count(handle.id)) {
            m_device.destroyRenderTarget(handle);
            m_legacyHandles.erase(handle.id);
        }

next_handle:;
    }

    m_deferredDestroy.clear();
    printf("[RTTManager] Deferred destroys complete.\n");
}

// ===========================================================================
// Legacy API (backward compatibility)
// ===========================================================================

ViewportHandle RTTManager::createCanvas(int width, int height) {

    // Use acquireCanvas with 2D default
    ViewportHandle h = acquireCanvas(width, height, RTType::RT_2D, true);
    if (h.id != 0) {
        m_legacyHandles.insert(h.id);
    }
    return h;
}

void RTTManager::destroyCanvas(ViewportHandle handle) {

    // Deferred destruction 鈥?flushed at end-of-frame by Engine::render()
    destroyCanvasDeferred(handle);
}

void RTTManager::clearAll() {

    for (auto& entry : m_pool2D) {
        if (entry.handle.id != 0) {
            m_device.destroyRenderTarget(entry.handle);
        }
    }
    for (auto& entry : m_pool3D) {
        if (entry.handle.id != 0) {
            m_device.destroyRenderTarget(entry.handle);
        }
    }
    for (uint32_t id : m_legacyHandles) {
        m_device.destroyRenderTarget(ViewportHandle{id});
    }

    m_pool2D.clear();
    m_pool3D.clear();
    m_handleToPoolIndex.clear();
    m_legacyHandles.clear();
    m_deferredDestroy.clear();

    printf("[RTTManager] All canvases cleared.\n");
}

// ===========================================================================
// Snapshot
// ===========================================================================

bool RTTManager::captureSnapshot(const char* path, int w, int h) {
    if (!path || w <= 0 || h <= 0) return false;
    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path);
    printf("[RTTManager] Snapshot requested: %s (%dx%d)\n", path, w, h);
    return true;
}

} // namespace Caesura