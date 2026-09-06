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

TEST_CASE("ParticleSystem::shutdown discards emitters without initialization") {
    ParticleSystem ps;
    const int firstId = ps.createEmitter(ParticleEmitterConfig{});
    const int secondId = ps.createEmitter(ParticleEmitterConfig{});

    CHECK(ps.activeEmitterCount() == 2);
    ps.shutdown();
    CHECK(ps.activeEmitterCount() == 0);

    CHECK_FALSE(ps.isInitialized());
    CHECK_FALSE(ps.destroyEmitter(firstId));
    CHECK_FALSE(ps.destroyEmitter(secondId));
    ps.emit(firstId, 5);
    ps.update(1.0f, 1280, 720);
    CHECK(ps.aliveCount() == 0);
}

TEST_CASE("ParticleSystem::shutdown starts a fresh emitter sequence") {
    ParticleSystem ps;
    const int firstId = ps.createEmitter(ParticleEmitterConfig{});
    const int secondId = ps.createEmitter(ParticleEmitterConfig{});
    REQUIRE(ps.destroyEmitter(firstId));
    CHECK(ps.activeEmitterCount() == 1);

    ps.shutdown();

    const int freshId = ps.createEmitter(ParticleEmitterConfig{});
    CHECK(freshId == firstId);
    CHECK_FALSE(ps.destroyEmitter(secondId));
    CHECK(ps.destroyEmitter(freshId));
    CHECK_FALSE(ps.destroyEmitter(freshId));
}

TEST_CASE("ParticleSystem::shutdown is idempotent across emitter lifetimes") {
    ParticleSystem ps;
    ps.shutdown();
    ps.shutdown();

    for (int cycle = 0; cycle < 3; ++cycle) {
        const int id = ps.createEmitter(ParticleEmitterConfig{});
        ps.shutdown();
        ps.shutdown();

        CHECK_FALSE(ps.isInitialized());
        CHECK_FALSE(ps.destroyEmitter(id));
        ps.emit(id, 1);
        ps.update(0.1f, 640, 480);
        CHECK(ps.aliveCount() == 0);
    }
}

TEST_CASE("ParticleSystem::aliveCount within MAX_PARTICLES") {
    Caesura::ParticleSystem ps;
    int cnt = ps.aliveCount(); CHECK(cnt <= 1024);
}
// -----------------------------------------------------------------------------
// Pure particle visual math (G8): lifeFade() decay curve + quad builder.
// No GPU, no init required -- pure functions on a Particle value.
// -----------------------------------------------------------------------------

TEST_CASE("Particle visual: fresh particle is full size and alpha") {
    Caesura::Particle p;
    p.x = 100.0f; p.y = 200.0f;
    p.size = 10.0f;
    p.life = 1.0f; p.maxLife = 1.0f;
    p.r = 1.0f; p.g = 0.5f; p.b = 0.0f; p.a = 1.0f;

    CHECK(Caesura::ParticleSystem::lifeFade(p) == doctest::Approx(1.0f));
    auto q = Caesura::ParticleSystem::buildParticleVisual(p);
    CHECK(q.x0 == doctest::Approx(95.0f));   // 100 - 10*1*0.5
    CHECK(q.y0 == doctest::Approx(195.0f));
    CHECK(q.x1 == doctest::Approx(105.0f));
    CHECK(q.y1 == doctest::Approx(205.0f));
    CHECK(q.r == 255);
    CHECK(q.g == 127);   // 0.5*255 = 127.5 -> 127
    CHECK(q.b == 0);
    CHECK(q.a == 255);
}

TEST_CASE("Particle visual: half-life shrinks quad and fades alpha") {
    Caesura::Particle p;
    p.x = 0.0f; p.y = 0.0f;
    p.size = 20.0f;
    p.life = 0.5f; p.maxLife = 1.0f;
    p.r = 1.0f; p.g = 1.0f; p.b = 1.0f; p.a = 1.0f;

    CHECK(Caesura::ParticleSystem::lifeFade(p) == doctest::Approx(0.5f));
    auto q = Caesura::ParticleSystem::buildParticleVisual(p);
    CHECK(q.x0 == doctest::Approx(-5.0f));   // 0 - 20*0.5*0.5
    CHECK(q.x1 == doctest::Approx(5.0f));
    CHECK(q.a == 127);   // 1.0*0.5*255 = 127.5 -> 127
}

TEST_CASE("Particle visual: near-death particle collapses") {
    Caesura::Particle p;
    p.x = 0.0f; p.y = 0.0f;
    p.size = 100.0f;
    p.life = 0.01f; p.maxLife = 1.0f;
    p.r = 1.0f; p.g = 1.0f; p.b = 1.0f; p.a = 1.0f;

    auto q = Caesura::ParticleSystem::buildParticleVisual(p);
    CHECK(q.x0 == doctest::Approx(-0.5f));
    CHECK(q.x1 == doctest::Approx(0.5f));
    CHECK(q.a == 2);   // 0.01*255 = 2.55 -> 2
}

TEST_CASE("Particle visual: zero maxLife does not produce NaN") {
    // Regression: life/maxLife with maxLife=0 is NaN; the guard must
    // degenerate to fade=1.0 so vertices stay finite.
    Caesura::Particle p;
    p.x = 50.0f; p.y = 60.0f;
    p.size = 8.0f;
    p.life = 0.0f; p.maxLife = 0.0f;
    p.r = 1.0f; p.g = 0.0f; p.b = 0.0f; p.a = 1.0f;

    CHECK(Caesura::ParticleSystem::lifeFade(p) == doctest::Approx(1.0f));
    auto q = Caesura::ParticleSystem::buildParticleVisual(p);
    CHECK(q.x0 == q.x0);   // not NaN
    CHECK(q.y1 == q.y1);
    CHECK(q.a == 255);
    CHECK(q.x0 == doctest::Approx(46.0f));
    CHECK(q.x1 == doctest::Approx(54.0f));
}

TEST_CASE("Particle visual: negative life clamps to zero fade") {
    Caesura::Particle p;
    p.x = 0.0f; p.y = 0.0f;
    p.size = 10.0f;
    p.life = -0.5f; p.maxLife = 1.0f;
    p.r = p.g = p.b = p.a = 1.0f;

    CHECK(Caesura::ParticleSystem::lifeFade(p) == doctest::Approx(0.0f));
    auto q = Caesura::ParticleSystem::buildParticleVisual(p);
    CHECK(q.a == 0);
    CHECK(q.x0 == doctest::Approx(0.0f));   // size 10 * fade 0 = 0
}

TEST_CASE("Particle visual: overshoot life clamps to full") {
    Caesura::Particle p;
    p.size = 10.0f;
    p.life = 2.0f; p.maxLife = 1.0f;
    p.r = p.g = p.b = p.a = 1.0f;

    CHECK(Caesura::ParticleSystem::lifeFade(p) == doctest::Approx(1.0f));
}
