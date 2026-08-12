// test_mesh_renderer.cpp - SMA mesh renderer interface tests (Battle 4d S1).
// Verifies the IMeshRenderer contract via the Null backend (no GPU):
// registry wiring, mesh create/destroy lifecycle, pose updates and draw
// calls are safe no-ops, and the engine accessor returns a working
// implementation. Real GPU rendering is deferred (see
// docs/solutions/deferred-gpu-tests.md).
#include "doctest.h"
#include "di/BackendRegistry.h"
#include "render/api/IRenderDevice.h"   // VIEW_MAIN
#include "render/api/IMeshRenderer.h"
#include "render/NullMeshRenderer.h"

using namespace Caesura;

TEST_CASE("NullMeshRenderer implements the interface") {
    NullMeshRenderer renderer;
    CHECK(renderer.isInitialized());
    CHECK(renderer.meshCount() == 0);
}

TEST_CASE("NullMeshRenderer create/destroy lifecycle") {
    NullMeshRenderer renderer;

    SMAMesh mesh;
    mesh.vertices = {
        {0.f, 0.f, 0.f, 0.f, 0, 1.f, 1, 0.f},
        {1.f, 0.f, 1.f, 0.f, 0, 1.f, 1, 0.f},
        {1.f, 1.f, 1.f, 1.f, 0, 1.f, 1, 0.f},
    };
    mesh.indices = {0, 1, 2};

    MeshHandle h = renderer.createMesh(mesh);
    CHECK(h);
    CHECK(renderer.meshCount() == 1);

    // pose update + draw are safe no-ops
    std::vector<BonePose> poses(2);
    poses[0].rot = 0.5f;
    poses[1].scale = 1.2f;
    CHECK_NOTHROW(renderer.updateMesh(h, poses));
    CHECK_NOTHROW(renderer.drawMesh(VIEW_MAIN, h, 1u, 0.5f, 0.5f, 1.f, 0.8f));

    renderer.destroyMesh(h);
    CHECK(renderer.meshCount() == 0);
}

TEST_CASE("BackendRegistry mesh renderer round-trip") {
    auto& reg = BackendRegistry::instance();
    NullMeshRenderer renderer;
    IMeshRenderer* old = reg.getMeshRenderer();
    reg.setMeshRenderer(&renderer);
    CHECK(reg.getMeshRenderer() == &renderer);
    reg.setMeshRenderer(old);  // restore
}

TEST_CASE("SMAMesh POD shape") {
    SMAMeshVertex v;
    CHECK(v.w0 == 0.f);
    CHECK(v.bone1 == UINT16_MAX);  // "no second bone" sentinel
    BonePose p;
    CHECK(p.scale == 1.f);
    CHECK(p.rot == 0.f);
}
