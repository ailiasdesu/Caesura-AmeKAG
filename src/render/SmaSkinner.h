#pragma once
#include "render/api/IMeshRenderer.h"
#include <cmath>
#include <vector>

namespace Caesura {

// ===========================================================================
//  SmaSkinner — CPU soft-skinning math (SMA Battle 4d S2).
//  Header-only pure functions: world-pose application + per-vertex weight
//  blending. Bone-hierarchy resolution (parent chain + pivot bake) lives in
//  the driver (scripts/kag/sma.lua) per the S1 interface contract
//  ("BonePose = world transform already resolved by the driver").
//
//  Deterministic and GPU-free: covered by doctest (test_sma_skinner.cpp).
// ===========================================================================

// Apply a world BonePose to a model-space point: rotate about the origin,
// uniform scale, then offset. The pivot is baked into the pose's offset by
// the driver, so no pivot parameter is needed here.
inline void applyBonePose(const BonePose& pose, float x, float y,
                          float& ox, float& oy) {
    const float c = std::cos(pose.rot);
    const float s = std::sin(pose.rot);
    const float sx = x * pose.scale;
    const float sy = y * pose.scale;
    ox = c * sx - s * sy + pose.ox;
    oy = s * sx + c * sy + pose.oy;
}

// Skinned GPU vertex (position + UV; weights are resolved on the CPU).
struct SmaSkinnedVertex {
    float x = 0.f, y = 0.f;
    float u = 0.f, v = 0.f;
};

// ---------------------------------------------------------------------------
//  S5: GPU bone packing. A world BonePose becomes one vec4
//  (m0, m1, ox, oy) with m0 = cos(rot)*scale, m1 = sin(rot)*scale, so the
//  compute shader applies exactly the CPU transform:
//    x' = m0*x - m1*y + ox
//    y' = m1*x + m0*y + oy
//  (identical to applyBonePose; unit-tested for bit-level agreement).
// ---------------------------------------------------------------------------
inline void packBonePose(const BonePose& pose, float out[4]) {
    const float c = std::cos(pose.rot);
    const float s = std::sin(pose.rot);
    out[0] = c * pose.scale;
    out[1] = s * pose.scale;
    out[2] = pose.ox;
    out[3] = pose.oy;
}

// Pack a pose array into the interleaved bone buffer layout (4 floats per
// bone; missing/out-of-range bones become the identity transform).
inline void packBonePoses(const std::vector<BonePose>& poses,
                          size_t boneCapacity,
                          std::vector<float>& out) {
    out.assign(boneCapacity * 4, 0.f);
    const size_t n = poses.size() < boneCapacity ? poses.size() : boneCapacity;
    for (size_t i = 0; i < n; ++i) {
        packBonePose(poses[i], &out[i * 4]);
    }
    // Identity rows for the rest: out[i] = {1, 0, 0, 0} -> x' = x.
    for (size_t i = n; i < boneCapacity; ++i) {
        out[i * 4 + 0] = 1.f;
    }
}

// Skin a mesh against world poses: per-vertex weighted blend (max 2 bones,
// weights normalized by their sum). `out` is resized to the vertex count.
// Vertices with no valid bone (or a zero weight sum) stay in place.
inline void skinMesh(const SMAMesh& mesh,
                     const std::vector<BonePose>& poses,
                     std::vector<SmaSkinnedVertex>& out) {
    out.resize(mesh.vertices.size());
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const SMAMeshVertex& v = mesh.vertices[i];
        const BonePose* p0 = (v.bone0 < poses.size()) ? &poses[v.bone0] : nullptr;
        const BonePose* p1 = (v.bone1 < poses.size()) ? &poses[v.bone1] : nullptr;

        float x, y;
        if (p0 && p1) {
            const float wsum = v.w0 + v.w1;
            if (wsum <= 0.f) {
                x = v.x;
                y = v.y;
            } else {
                float x0, y0, x1, y1;
                applyBonePose(*p0, v.x, v.y, x0, y0);
                applyBonePose(*p1, v.x, v.y, x1, y1);
                x = (x0 * v.w0 + x1 * v.w1) / wsum;
                y = (y0 * v.w0 + y1 * v.w1) / wsum;
            }
        } else if (p0) {
            applyBonePose(*p0, v.x, v.y, x, y);
        } else {
            x = v.x;
            y = v.y;
        }
        out[i] = { x, y, v.u, v.v };
    }
}

} // namespace Caesura
