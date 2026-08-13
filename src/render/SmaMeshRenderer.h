#pragma once
#include "render/api/IMeshRenderer.h"
#include "SmaSkinner.h"
#include <bgfx/bgfx.h>
#include <memory>
#include <vector>

namespace Caesura {

class BgfxShaderManager;

// ===========================================================================
//  SmaMeshRenderer — real bgfx implementation of IMeshRenderer (SMA S2/S5).
//
//  S2: CPU soft-skinning (SmaSkinner) + transient-vertex-buffer draw reusing
//  the existing pos+uv layout and the embedded texture program.
//
//  S5: GPU compute skinning — per-mesh compute input/output vertex buffers
//  (positions+UV+bones+weights in, skinned positions+UV out) and a shared
//  bone transform buffer, driven by a compute shader (D3D11 DXBC embedded;
//  GL 430 GLSL embedded). SkinMode::Auto picks the GPU path when the backend
//  reports BGFX_CAPS_COMPUTE (D3D11/GL); Metal/SPIR-V/Noop fall back to the
//  CPU skinner (see docs/design/skeletal-mesh-animation.md §3.1/S5).
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

    void setSkinMode(SkinMode mode) override;
    SkinMode skinMode() const override { return m_skinMode; }

    // True when the S5 compute skin pipeline is usable on this backend
    // (program + bone buffer built; not a per-mesh check). Lets tests and
    // drivers probe capability instead of silently falling back to CPU.
    bool gpuSkinAvailable() const {
        return m_initialized
            && bgfx::isValid(m_skinProgram)
            && bgfx::isValid(m_boneBuffer);
    }

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
        // S5 GPU skinning resources (valid when gpuSkinReady).
        // gpuIn is STATIC (D3D11 forbids DYNAMIC usage with an SRV bind).
        bgfx::VertexBufferHandle gpuIn = BGFX_INVALID_HANDLE;
        bgfx::DynamicVertexBufferHandle gpuOut = BGFX_INVALID_HANDLE;
        // Poses stored by updateMesh; packed + dispatched at draw time
        // (same-view ordering: the bone upload + dispatch must precede
        // the draw submit that consumes the output buffer).
        std::vector<BonePose> pendingPoses;
        bool gpuDirty = false;   // pendingPoses not yet skinned on GPU
        bool gpuSkinned = false; // gpuOut holds a valid skin
        bool gpuSkinReady = false;
        bool gpuReady = false;
    };

    MeshEntry* find(MeshHandle handle);

    // Effective mode for a given mesh: Auto resolves against the backend
    // caps (compute supported + D3D11/GL renderer).
    bool useGpuSkin(const MeshEntry& entry) const;
    void skinOnGpu(MeshEntry& entry, const std::vector<BonePose>& poses,
                   uint16_t targetView, float x, float y, float scale,
                   float viewW, float viewH);
    void skinOnCpu(MeshEntry& entry, const std::vector<BonePose>& poses);

    bgfx::VertexLayout m_layout;      // output: pos + uv
    bgfx::VertexLayout m_skinLayout;  // input: pos + uv + bones + weights
    bgfx::DynamicVertexBufferHandle m_boneBuffer = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_skinProgram = BGFX_INVALID_HANDLE;
    std::unique_ptr<BgfxShaderManager> m_shaders;
    std::vector<MeshEntry> m_meshes;
    uint32_t m_nextId = 1;
    SkinMode m_skinMode = SkinMode::Auto;
    mutable bool m_skinWarningShown = false;  // one-time Gpu-force fallback note
    bool m_initialized = false;
};

} // namespace Caesura