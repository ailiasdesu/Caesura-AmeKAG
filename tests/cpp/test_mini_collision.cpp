// test_mini_collision.cpp - sweep-and-prune collision detection tests.
#include "doctest.h"
#include "minigame/MiniCollision.h"

using namespace Caesura;

TEST_CASE("findCollisions detects overlapping AABBs") {
    // Two overlapping boxes at x=0 and x=1 (size 2 -> half-extent 1).
    uint32_t ids[2] = { 7, 9 };
    float px[2] = { 0.0f, 1.0f };
    float py[2] = { 0.0f, 0.0f };
    float pz[2] = { 0.0f, 0.0f };
    float sx[2] = { 2.0f, 2.0f };
    float sy[2] = { 2.0f, 2.0f };
    float sz[2] = { 2.0f, 2.0f };
    auto pairs = findCollisions(ids, px, py, pz, sx, sy, sz, 2);
    REQUIRE(pairs.size() == 1);
    const bool ordered = (pairs[0].first == 7 && pairs[0].second == 9)
                      || (pairs[0].first == 9 && pairs[0].second == 7);
    CHECK(ordered);
}

TEST_CASE("findCollisions prunes separated boxes") {
    // Same plane, X gap of 10 -> no collision despite Y/Z overlap.
    uint32_t ids[2] = { 1, 2 };
    float px[2] = { 0.0f, 10.0f };
    float py[2] = { 0.0f, 0.0f };
    float pz[2] = { 0.0f, 0.0f };
    float sx[2] = { 2.0f, 2.0f };
    float sy[2] = { 2.0f, 2.0f };
    float sz[2] = { 2.0f, 2.0f };
    auto pairs = findCollisions(ids, px, py, pz, sx, sy, sz, 2);
    CHECK(pairs.empty());
}

TEST_CASE("findCollisions large sparse set (perf guard)") {
    // 200 boxes spread along X with no overlap: sweep-and-prune must be
    // fast AND return zero pairs (naive O(n^2) would test 19900 pairs).
    constexpr size_t N = 200;
    uint32_t ids[N];
    float px[N], py[N], pz[N], sx[N], sy[N], sz[N];
    for (size_t i = 0; i < N; ++i) {
        ids[i] = static_cast<uint32_t>(i);
        px[i] = static_cast<float>(i * 4);  // gap 4 > half-extent 1
        py[i] = pz[i] = 0.0f;
        sx[i] = sy[i] = sz[i] = 2.0f;
    }
    auto pairs = findCollisions(ids, px, py, pz, sx, sy, sz, N);
    CHECK(pairs.empty());
}

TEST_CASE("findCollisions dense set finds all overlaps") {
    // 50 boxes all at the origin: every pair collides (1225 pairs).
    constexpr size_t N = 50;
    uint32_t ids[N];
    float px[N], py[N], pz[N], sx[N], sy[N], sz[N];
    for (size_t i = 0; i < N; ++i) {
        ids[i] = static_cast<uint32_t>(i);
        px[i] = py[i] = pz[i] = 0.0f;
        sx[i] = sy[i] = sz[i] = 2.0f;
    }
    auto pairs = findCollisions(ids, px, py, pz, sx, sy, sz, N);
    CHECK(pairs.size() == N * (N - 1) / 2);
}
