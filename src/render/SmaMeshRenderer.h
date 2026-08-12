#pragma once
#include "render/api/IMeshRenderer.h"
#include "SmaSkinner.h"
#include <bgfx/bgfx.h>
#include <memory>
#include <vector>

namespace Caesura {

class BgfxShaderManager;

// ===========================================================================
//  SmaMeshRenderer — real bgfx implementation of IMeshRenderer (SMA S2).
//  CPU soft-skinning (SmaSkinner) + transient-vertex-buffer draw reusing
//  the existing pos+uv layout and the embedded texture program — zero new
//  shaders, matching docs/design/skeletal-mesh-animation.md §3.1.
//
//  GPU-free environments (headless tests, CI): init() is skipped and every
//  operation is a safe no-op (deferred-gpu pattern) — createMesh returns an
//  invalid handle, meshCount() still tracks bookkeeping when initialized.
// ===========================================================================

class SmaMeshRenderer final : public IMeshRenderer {
public:
    SmaMeshRenderer(); // defined out-of-line (owns BgfxShaderManager)
    ~SmaMeshRenderer() override;

    // Lazy init once bgfx is up (called from createMesh/drawMesh paths).
    void init();

    bool isInitialized() const override { return m_initialized; }

    MeshHandle createMesh(const SMAMesh& mesh) override;
    void destroyMesh(MeshHandle handle) override;

    void updateMesh(MeshHandle handle,
                    const std::vector<BonePose>& poses) override;

    void drawMesh(uint16_t targetView, MeshHandle handle,
                  uint32_t dstTexId, float x, float y,
                  float scale, float opacity) override;

    size_t meshCount() const override { return m_meshes.size(); }

private:
    struct MeshEntry {
        MeshHandle handle;
        SMAMesh mesh; // CPU copy (skinning input)
        bgfx::IndexBufferHandle ib;
        std::vector<SmaSkinnedVertex> skinned;
        bool gpuReady = false;
    };

    MeshEntry* find(MeshHandle handle);

    bgfx::VertexLayout m_layout;
    std::unique_ptr<BgfxShaderManager> m_shaders;
    std::vector<MeshEntry> m_meshes;
    uint32_t m_nextId = 1;
    bool m_initialized = false;
};

} // namespace Caesura
