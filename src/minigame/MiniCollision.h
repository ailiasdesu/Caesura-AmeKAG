#pragma once
#include <cstdint>
#include <vector>
#include <utility>

namespace Caesura {

// ===========================================================================
// MiniCollision — AABB collision detection for mini-game objects
// ===========================================================================

struct AABB {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
};

// Collision pair: two object IDs that overlap
using CollisionPair = std::pair<uint32_t, uint32_t>;

// Compute AABB from position + scale (unit geometry scaled)
// Each geometry type treated as a box of size scaleX×scaleY×scaleZ
AABB computeAABB(float posX, float posY, float posZ,
                 float scaleX, float scaleY, float scaleZ);

// Test overlap between two AABBs (inclusive edges)
bool aabbOverlap(const AABB& a, const AABB& b);

// Brute-force collision check: returns all overlapping pairs
// O(n²), fine for <100 objects
std::vector<CollisionPair> findCollisions(
    const uint32_t* objIds, const float* posX, const float* posY, const float* posZ,
    const float* scaleX, const float* scaleY, const float* scaleZ,
    size_t count);

} // namespace Caesura
