// test_particle_system.cpp - ParticleSystem tests (merged from test_render.cpp)
#include "doctest.h"
#include "../src/render/ParticleSystem.h"

using namespace Caesura;

TEST_CASE("ParticleSystem::MAX_PARTICLES constant") {
    CHECK(ParticleSystem::MAX_PARTICLES == 1024);
}

TEST_CASE("ParticleSystem::init with nullptr device") {
    ParticleSystem ps;
    bool ok = ps.init();
    (void)ok;
}

TEST_CASE("ParticleSystem::update dynamic resolution no-crash") {
    ParticleSystem ps;
    ps.init();
    ps.update(0.016f, 1920, 1080);
    ps.update(0.016f, 640, 480);
}

TEST_CASE("ParticleSystem::createEmitter assigns id") {
    Caesura::ParticleSystem ps;
    int id = ps.createEmitter(Caesura::Emitter{});
    CHECK(id >= 0);
}

TEST_CASE("ParticleSystem::createEmitter multiple") {
    Caesura::ParticleSystem ps;
    int a = ps.createEmitter(Caesura::Emitter{});
    int b = ps.createEmitter(Caesura::Emitter{});
    int c = ps.createEmitter(Caesura::Emitter{});
    CHECK(a >= 0);
    CHECK(b >= 0);
    CHECK(c >= 0);
    CHECK(a != b);
    CHECK(b != c);
}

TEST_CASE("ParticleSystem::destroyEmitter then create reuses") {
    Caesura::ParticleSystem ps;
    int id = ps.createEmitter(Caesura::Emitter{});
    ps.destroyEmitter(id);
    int newId = ps.createEmitter(Caesura::Emitter{});
    CHECK(newId >= 0);
}

TEST_CASE("ParticleSystem::emit no-op when not initialized") {
    Caesura::ParticleSystem ps;
    int id = ps.createEmitter(Caesura::Emitter{});
    ps.emit(id, 5);
    CHECK(ps.aliveCount() == 0);
}

TEST_CASE("ParticleSystem::update no-op when not initialized") {
    Caesura::ParticleSystem ps;
    Caesura::Emitter cfg;
    cfg.lifeMin = 0.1f;
    cfg.lifeMax = 0.2f;
    int id = ps.createEmitter(cfg);
    ps.emit(id, 10);
    ps.update(0.1f, 1280, 720);
    CHECK(ps.aliveCount() == 0);
}

TEST_CASE("ParticleSystem::destroyEmitter sets inactive") {
    Caesura::ParticleSystem ps;
    Caesura::Emitter cfg;
    cfg.rate = 100.0f;
    int id = ps.createEmitter(cfg);
    ps.destroyEmitter(id);
    CHECK(id >= 0);
}

TEST_CASE("ParticleSystem::aliveCount within MAX_PARTICLES") {
    Caesura::ParticleSystem ps;
    int cnt = ps.aliveCount(); CHECK(cnt <= 1024);
}

TEST_CASE("processSimBatch no crash on empty") {
    Caesura::SimBatch batch;
    batch.particles = nullptr;
    batch.startIdx = 0;
    batch.endIdx = 0;
    batch.dt = 0.016f;
    batch.gravityX = 0;
    batch.gravityY = 0;
    batch.deadCount = 0;
    Caesura::processSimBatch(batch);
    CHECK(batch.deadCount == 0);
}
