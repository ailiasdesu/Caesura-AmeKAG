// test_render_integration.cpp - render pipeline integration tests
#include "doctest.h"
#include "render/BgfxRenderDevice.h"
#include "render/ParticleSystem.h"
#include "render/FreeTypeContext.h"

using namespace Caesura;

TEST_CASE("Render: device default values") {
    BgfxRenderDevice rd;
    CHECK(rd.getBackbufferWidth() == 1280);
    CHECK(rd.getBackbufferHeight() == 720);
    CHECK(rd.getBackendName() != nullptr);
}

TEST_CASE("Render: device double shutdown idempotent") {
    BgfxRenderDevice rd;
    rd.shutdown();
    rd.shutdown();
    CHECK(rd.getBackendName() != nullptr);
}

TEST_CASE("Render: ParticleSystem create/destroy emitter") {
    ParticleSystem ps;
    Emitter cfg;
    int eid = ps.createEmitter(cfg);
    CHECK(eid >= 0);
    ps.destroyEmitter(eid);
    ps.destroyEmitter(eid);
}

TEST_CASE("Render: ParticleSystem update with no emitters") {
    ParticleSystem ps;
    ps.update(0.016f, 1280, 720);
    CHECK(ps.aliveCount() <= 1024);
}

TEST_CASE("Render: ParticleSystem multiple create/destroy") {
    ParticleSystem ps;
    Emitter cfg;
    int e1 = ps.createEmitter(cfg);
    int e2 = ps.createEmitter(cfg);
    CHECK(e1 >= 0);
    CHECK(e2 >= 0);
    CHECK(e1 != e2);
    ps.destroyEmitter(e1);
    ps.destroyEmitter(e2);
}

TEST_CASE("Render: ParticleSystem emit without init") {
    ParticleSystem ps;
    Emitter cfg;
    int eid = ps.createEmitter(cfg);
    ps.emit(eid, 5);
    ps.update(0.1f, 1280, 720);
    ps.destroyEmitter(eid);
}

TEST_CASE("Render: FreeTypeContext init/shutdown cycle") {
    auto& ft = FreeTypeContext::instance();
    CHECK(ft.init());
    ft.shutdown();
    ft.shutdown();
}
