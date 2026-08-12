// test_sma_skinner.cpp - SMA CPU soft-skinning math tests (Battle 4d S2).
// Pure, deterministic, GPU-free: world-pose application + per-vertex
// weight blending (SmaSkinner.h) plus the SmaMeshRenderer no-GPU contract
// (deferred-gpu pattern: without bgfx every operation is a safe no-op).
#include "doctest.h"
#include "render/SmaSkinner.h"
#include "render/SmaMeshRenderer.h"
#include <cmath>

using namespace Caesura;

namespace {
constexpr float kEps = 1e-4f;

SMAMesh makeQuad() {
    SMAMesh mesh;
    mesh.vertices = {
        {0.f, 0.f, 0.f, 0.f, 0, 1.f, 1, 0.f},
        {1.f, 0.f, 1.f, 0.f, 0, 1.f, 1, 0.f},
        {1.f, 1.f, 1.f, 1.f, 0, 1.f, 1, 0.f},
        {0.f, 1.f, 0.f, 1.f, 0, 1.f, 1, 0.f},
    };
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}
} // namespace

TEST_CASE("applyBonePose rotates about the origin") {
    BonePose pose;
    pose.rot = 3.14159265f / 2.f; // 90 degrees CCW
    float x, y;
    applyBonePose(pose, 1.f, 0.f, x, y);
    CHECK(std::fabs(x - 0.f) < kEps);
    CHECK(std::fabs(y - 1.f) < kEps);
}

TEST_CASE("applyBonePose scales and offsets") {
    BonePose pose;
    pose.scale = 2.f;
    pose.ox = 10.f;
    pose.oy = -3.f;
    float x, y;
    applyBonePose(pose, 1.f, 1.f, x, y);
    CHECK(std::fabs(x - 12.f) < kEps);
    CHECK(std::fabs(y + 1.f) < kEps); // 1*2 + (-3) = -1
}

TEST_CASE("skinMesh single-bone mesh") {
    SMAMesh mesh = makeQuad();
    std::vector<BonePose> poses(2);
    poses[0].ox = 5.f; // translate only
    std::vector<SmaSkinnedVertex> out;
    skinMesh(mesh, poses, out);
    REQUIRE(out.size() == 4);
    CHECK(std::fabs(out[0].x - 5.f) < kEps);
    CHECK(std::fabs(out[2].x - 6.f) < kEps); // 1 + 5
    CHECK(std::fabs(out[2].u - 1.f) < kEps); // UV untouched
    CHECK(std::fabs(out[2].v - 1.f) < kEps);
}

TEST_CASE("skinMesh blends two weighted bones") {
    SMAMesh mesh = makeQuad();
    // vertex 0: 50/50 between bone0 (+10 x) and bone1 (+20 x) -> +15
    mesh.vertices[0].bone0 = 0;
    mesh.vertices[0].w0 = 0.5f;
    mesh.vertices[0].bone1 = 1;
    mesh.vertices[0].w1 = 0.5f;
    std::vector<BonePose> poses(2);
    poses[0].ox = 10.f;
    poses[1].ox = 20.f;
    std::vector<SmaSkinnedVertex> out;
    skinMesh(mesh, poses, out);
    CHECK(std::fabs(out[0].x - 15.f) < kEps);
}

TEST_CASE("skinMesh weight normalization (25/75)") {
    SMAMesh mesh = makeQuad();
    mesh.vertices[0].bone0 = 0;
    mesh.vertices[0].w0 = 1.f;
    mesh.vertices[0].bone1 = 1;
    mesh.vertices[0].w1 = 3.f; // sum 4 -> normalized 0.25 / 0.75
    std::vector<BonePose> poses(2);
    poses[0].ox = 0.f;
    poses[1].ox = 4.f;
    std::vector<SmaSkinnedVertex> out;
    skinMesh(mesh, poses, out);
    CHECK(std::fabs(out[0].x - 3.f) < kEps); // 0.25*0 + 0.75*4
}

TEST_CASE("skinMesh zero-weight vertex stays in place") {
    SMAMesh mesh = makeQuad();
    mesh.vertices[1].bone0 = 0;
    mesh.vertices[1].w0 = 0.f;
    mesh.vertices[1].bone1 = 1;
    mesh.vertices[1].w1 = 0.f;
    std::vector<BonePose> poses(2);
    poses[0].ox = 100.f;
    poses[1].ox = 200.f;
    std::vector<SmaSkinnedVertex> out;
    skinMesh(mesh, poses, out);
    CHECK(std::fabs(out[1].x - 1.f) < kEps);
    CHECK(std::fabs(out[1].y - 0.f) < kEps);
}

TEST_CASE("skinMesh out-of-range bone index is identity") {
    SMAMesh mesh = makeQuad();
    mesh.vertices[0].bone0 = 7; // no pose for bone 7
    std::vector<BonePose> poses(1);
    poses[0].ox = 9.f;
    std::vector<SmaSkinnedVertex> out;
    skinMesh(mesh, poses, out);
    CHECK(std::fabs(out[0].x - 0.f) < kEps);
    CHECK(std::fabs(out[0].y - 0.f) < kEps);
}

TEST_CASE("SmaMeshRenderer no-GPU contract (deferred)") {
    SmaMeshRenderer renderer; // bgfx not initialized in unit tests
    CHECK_FALSE(renderer.isInitialized());
    CHECK(renderer.meshCount() == 0);

    SMAMesh mesh = makeQuad();
    // createMesh without GPU: lazy init bails -> invalid handle, no crash
    MeshHandle h = renderer.createMesh(mesh);
    CHECK_FALSE(h);
    CHECK(renderer.meshCount() == 0);

    std::vector<BonePose> poses(1);
    poses[0].ox = 1.f;
    CHECK_NOTHROW(renderer.updateMesh(h, poses));
    CHECK_NOTHROW(renderer.destroyMesh(h));
    CHECK_NOTHROW(renderer.drawMesh(0, h, 1, 0.f, 0.f, 1.f, 1.f));
}
