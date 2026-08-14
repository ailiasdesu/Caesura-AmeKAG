#include "RTTManager.h"
#include <stb/stb_image_write.h>   // declarations only; impl lives in stb_impl.cpp
#include "di/BackendRegistry.h"
#include "../debug/api/DebugLog.h"   // P1-6: api header instead of concrete DebugManager.h
#include <bgfx/bgfx.h>
#include <vector>
#include <cstdio>

namespace Caesura {

// ===========================================================================
// Lifecycle
// ===========================================================================

RTTManager::RTTManager(IRenderDevice& device) : m_device(device) {
    BackendRegistry::instance().registerDeviceLostListener(this);
}

RTTManager::~RTTManager() {
    BackendRegistry::instance().unregisterDeviceLostListener(this);
}

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
    // Route through IRenderDevice so the clear path is GPU-free in tests
    // (the mock device implements these as no-ops; BgfxRenderDevice forwards
    // them to the same bgfx calls as before).
    m_device.setViewRect((uint16_t)entry.handle.id, 0, 0,
                         (uint16_t)entry.width, (uint16_t)entry.height);
    m_device.setViewClear((uint16_t)entry.handle.id,
                          BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                          0x00000000, 1.0f, 0);
    m_device.touch((uint16_t)entry.handle.id);
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
        DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                  "[RTTManager] Failed to create %s RTT (%dx%d)",
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
    m_handleToPoolIndex[hdl.id] = PoolSlot{type, pool.size() - 1};

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
        // Fast path: the map records which pool owns this handle.
        auto& pool = poolFor(it->second.type);
        size_t idx = it->second.index;
        if (idx < pool.size() && pool[idx].handle.id == handle.id) {
            pool[idx].inUse = false;
            printf("[RTTManager] Released %s RTT %u back to pool (%dx%d)\n",
                   (it->second.type == RTType::RT_3D ? "3D" : "2D"),
                   handle.id, pool[idx].width, pool[idx].height);
            return;
        }
        // Stale index (shouldn't happen) -- drop the map entry and fall through.
        m_handleToPoolIndex.erase(it);
    }

    // Not in pool -- fallback: just mark as available in whichever pool matches
    DEBUG_ERR(SubSys::Render, ErrCode::Ok,
              "[RTTManager] releaseCanvas: handle %u not found in pool index, searching...", handle.id);
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

        // Legacy handles are ALSO pooled (createCanvas -> acquireCanvas), so the
        // GPU target is destroyed exactly once by the pool path below; clear the
        // legacy marker here so clearAll() does not double-destroy.
        m_legacyHandles.erase(handle.id);

        // Pool lookup: handle -> (pool, index)
        auto it = m_handleToPoolIndex.find(handle.id);
        if (it != m_handleToPoolIndex.end()) {
            auto& pool = poolFor(it->second.type);
            size_t idx = it->second.index;
            if (idx < pool.size() && pool[idx].handle.id == handle.id) {
                m_device.destroyRenderTarget(pool[idx].handle);
                pool.erase(pool.begin() + static_cast<std::ptrdiff_t>(idx));
                // Fixup: only entries in the SAME pool shift left. Decrementing
                // every map entry would corrupt the other pool's indices.
                const RTType erasedType = it->second.type;
                m_handleToPoolIndex.erase(it);
                for (auto& kv : m_handleToPoolIndex) {
                    if (kv.second.type == erasedType && kv.second.index > idx) {
                        --kv.second.index;
                    }
                }
                continue;
            }
            // Stale index (shouldn't happen) -- drop the map entry and fall
            // through to the linear search below.
            m_handleToPoolIndex.erase(it);
        }

        // Fallback: linear search both pools
        for (auto& pool : {&m_pool2D, &m_pool3D}) {
            for (auto it2 = pool->begin(); it2 != pool->end(); ++it2) {
                if (it2->handle.id == handle.id) {
                    m_device.destroyRenderTarget(it2->handle);
                    pool->erase(it2);
                    break;
                }
            }
        }
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

    // Deferred destruction — flushed at end-of-frame by Engine::render()
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

void RTTManager::resizeAll(int /*newW*/, int /*newH*/) {
    // Pooled canvases are re-created at the new size on demand by acquire()
    // (the pool is keyed by (w,h) at acquire time), so a resize only needs
    // to drop every pooled/legacy canvas. The actual backbuffer dimensions
    // are owned by the render device; RTTManager never stores them.
    // (Round 40+1: implementation was missing - declared in the header but
    // never defined, which broke linking for the new pool tests.)
    clearAll();
}

bool RTTManager::captureSnapshot(const char* path, int w, int h) {
    if (!path || w <= 0 || h <= 0) return false;
    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path);
    printf("[RTTManager] Snapshot requested: %s (%dx%d)\n", path, w, h);
    return true;
}

} // namespace Caesura