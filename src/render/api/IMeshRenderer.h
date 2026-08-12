#pragma once
#include <cstdint>
#include <vector>

namespace Caesura {

// ===========================================================================
//  IMeshRenderer — skeletal mesh animation renderer (SMA, Battle 4d S1).
//  Pure-virtual interface, no data members (AGENTS.md §2). The POD mesh
//  types live here (used by value/const-ref in interface methods).
//
//  Purpose: render 2D skeletal-mesh characters (E-mote-class animation)
//  — CPU soft-skinning of vertex positions from bone poses, drawn with
//  the existing texture pipeline. See docs/design/skeletal-mesh-animation.md.
// ===========================================================================

// Opaque mesh handle (created by createMesh).
struct MeshHandle {
    uint32_t id = 0;
    explicit operator bool() const { return id != 0; }
    bool operator==(const MeshHandle& o) const { return id == o.id; }
    bool operator!=(const MeshHandle& o) const { return id != o.id; }
};

// One vertex: position + UV + per-bone weights (max 2 bones).
struct SMAMeshVertex {
    float x = 0.f, y = 0.f;   // model-space position
    float u = 0.f, v = 0.f;   // atlas UV
    uint16_t bone0 = 0;       // bone index 0 (weight w0)
    float w0 = 0.f;
    uint16_t bone1 = UINT16_MAX;  // bone index 1 (weight w1); UINT16_MAX = none
    float w1 = 0.f;
};

// Mesh description uploaded via createMesh (CPU-copy; the renderer owns
// the GPU-side buffers).
struct SMAMesh {
    std::vector<SMAMeshVertex> vertices;
    std::vector<uint16_t> indices;      // triangle list (multiple of 3)
};

// Bone pose at a point in time (world transform already resolved by the
// driver: rotation radians + scale + offset in normalized units).
struct BonePose {
    float rot = 0.f;      // radians
    float scale = 1.f;    // uniform scale
    float ox = 0.f;       // offset x (normalized 0..1)
    float oy = 0.f;       // offset y (normalized 0..1)
};

class IMeshRenderer {
public:
    virtual ~IMeshRenderer() = default;

    // -- Lifecycle ---------------------------------------------------------
    virtual bool isInitialized() const = 0;

    // -- Mesh upload / release --------------------------------------------
    virtual MeshHandle createMesh(const SMAMesh& mesh) = 0;
    virtual void destroyMesh(MeshHandle handle) = 0;

    // -- Per-frame pose update (CPU soft-skinning happens here) -----------
    // `poses` maps bone index -> world pose; vertices are re-skinned and
    // the GPU buffers updated.
    virtual void updateMesh(MeshHandle handle,
                            const std::vector<BonePose>& poses) = 0;

    // -- Draw --------------------------------------------------------------
    // Draws the skinned mesh into `targetView` at (x, y) with scale and
    // opacity, sampling `dstTexId` (an engine render texture handle id).
    virtual void drawMesh(uint16_t targetView, MeshHandle handle,
                          uint32_t dstTexId, float x, float y,
                          float scale, float opacity) = 0;

    // -- Debug/state -------------------------------------------------------
    virtual size_t meshCount() const = 0;
};

}  // namespace Caesura
