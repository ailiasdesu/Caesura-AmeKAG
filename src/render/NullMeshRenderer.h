#pragma once
#include "render/api/IMeshRenderer.h"

namespace Caesura {

// NullMeshRenderer — no-op implementation for SDK-less / headless
// environments (test, CI without GPU). All operations are safe no-ops;
// meshCount() tracks uploads so tests can verify the pipeline wiring
// without a GPU (deferred-gpu pattern).

class NullMeshRenderer final : public IMeshRenderer {
public:
    NullMeshRenderer() = default;
    ~NullMeshRenderer() override = default;

    bool isInitialized() const override { return true; }

    MeshHandle createMesh(const SMAMesh& mesh) override;
    void destroyMesh(MeshHandle handle) override;

    void updateMesh(MeshHandle handle,
                    const std::vector<BonePose>& poses) override;

    void drawMesh(uint16_t targetView, MeshHandle handle,
                  uint32_t dstTexId, float x, float y,
                  float scale, float opacity) override;

    size_t meshCount() const override { return m_meshes.size(); }

private:
    std::vector<MeshHandle> m_meshes;
    uint32_t m_nextId = 1;
};

}  // namespace Caesura
