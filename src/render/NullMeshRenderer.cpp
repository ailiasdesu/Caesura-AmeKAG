// NullMeshRenderer.cpp — no-op SMA renderer for SDK-less environments.
#include "NullMeshRenderer.h"

namespace Caesura {

MeshHandle NullMeshRenderer::createMesh(const SMAMesh& mesh) {
    MeshHandle h;
    h.id = m_nextId++;
    m_meshes.push_back(h);
    return h;
}

void NullMeshRenderer::destroyMesh(MeshHandle handle) {
    for (auto it = m_meshes.begin(); it != m_meshes.end(); ++it) {
        if (it->id == handle.id) {
            m_meshes.erase(it);
            return;
        }
    }
}

void NullMeshRenderer::updateMesh(MeshHandle,
                                  const std::vector<BonePose>&) {
    // no-op: no GPU buffers in the null backend
}

void NullMeshRenderer::drawMesh(uint16_t, MeshHandle, uint32_t, float,
                                float, float, float) {
    // no-op: nothing to draw without a GPU
}

}  // namespace Caesura
