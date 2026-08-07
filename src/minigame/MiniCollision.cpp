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
    // Sweep-and-prune on the X axis: sort by minX once, then test only
    // pairs whose X intervals overlap -- O(n log n + k) instead of the
    // naive O(n^2) all-pairs test. AABBs with disjoint X intervals cannot
    // overlap, so the pruning is exact (same pair set, different order).
    struct Item {
        uint32_t id;
        AABB     box;
    };
    std::vector<Item> items;
    items.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        items.push_back(
            { ids[i], computeAABB(px[i], py[i], pz[i], sx[i], sy[i], sz[i]) });
    }
    std::sort(items.begin(), items.end(),
              [](const Item& a, const Item& b) {
                  return a.box.minX < b.box.minX;
              });

    std::vector<CollisionPair> pairs;
    for (size_t i = 0; i < items.size(); ++i) {
        const AABB& ai = items[i].box;
        for (size_t j = i + 1; j < items.size(); ++j) {
            if (items[j].box.minX > ai.maxX) break;  // sweep prune
            if (aabbOverlap(ai, items[j].box)) {
                pairs.emplace_back(items[i].id, items[j].id);
            }
        }
    }
    return pairs;
}

} // namespace Caesura
