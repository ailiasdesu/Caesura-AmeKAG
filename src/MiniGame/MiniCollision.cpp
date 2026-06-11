#include "MiniCollision.h"
#include <algorithm>

namespace Caesura {

AABB computeAABB(float px, float py, float pz,
                 float sx, float sy, float sz) {
    float hx = sx * 0.5f;
    float hy = sy * 0.5f;
    float hz = sz * 0.5f;
    return { px - hx, py - hy, pz - hz, px + hx, py + hy, pz + hz };
}

bool aabbOverlap(const AABB& a, const AABB& b) {
    return (a.minX <= b.maxX && a.maxX >= b.minX) &&
           (a.minY <= b.maxY && a.maxY >= b.minY) &&
           (a.minZ <= b.maxZ && a.maxZ >= b.minZ);
}

std::vector<CollisionPair> findCollisions(
    const uint32_t* ids, const float* px, const float* py, const float* pz,
    const float* sx, const float* sy, const float* sz, size_t count) {
    std::vector<CollisionPair> pairs;
    for (size_t i = 0; i < count; ++i) {
        AABB ai = computeAABB(px[i], py[i], pz[i], sx[i], sy[i], sz[i]);
        for (size_t j = i + 1; j < count; ++j) {
            AABB aj = computeAABB(px[j], py[j], pz[j], sx[j], sy[j], sz[j]);
            if (aabbOverlap(ai, aj)) {
                pairs.emplace_back(ids[i], ids[j]);
            }
        }
    }
    return pairs;
}

} // namespace Caesura
