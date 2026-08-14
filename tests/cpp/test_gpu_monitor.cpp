// test_gpu_monitor.cpp - GpuMonitor adaptive-quality state machine tests.
// GPU-free: bgfx::getStats() is not callable before bgfx::init() (it
// dereferences the context), so GpuMonitor gates it behind setGpuAvailable();
// with the flag off (default, as in tests) update() falls back to CPU dt and
// the degradation/recovery machine is fully exercisable (round 39).
#include "doctest.h"
#include "render/GpuMonitor.h"

using namespace Caesura;

TEST_CASE("GpuMonitor starts at HIGH with clean metrics") {
    GpuMonitor monitor;
    CHECK(monitor.currentQuality() == GpuQuality::HIGH);
    CHECK_FALSE(monitor.isDegraded());
    CHECK(monitor.vfxEnabled());
    const auto& m = monitor.metrics();
    CHECK(m.frameCount == 0);
    CHECK(m.quality == GpuQuality::HIGH);
}

TEST_CASE("GpuMonitor update in headless env does not crash and falls back to CPU dt") {
    GpuMonitor monitor;
    // dt of 0.01s = 10ms cpu; without bgfx stats the gpuTime falls back to
    // cpu dt (10ms), which is under the 16.67ms budget.
    const GpuQuality q = monitor.update(0.01);
    CHECK(q == GpuQuality::HIGH);
    CHECK(monitor.metrics().frameCount == 1);
    CHECK(monitor.metrics().cpuTimeMs == doctest::Approx(10.0));
}

TEST_CASE("GpuMonitor degrades after 3 consecutive over-budget frames") {
    GpuMonitor monitor;
    // 20ms per frame > 16.67ms budget. Headless fallback uses cpu dt, so
    // dt = 0.02 drives the degradation machine directly.
    for (int i = 0; i < 3; ++i) monitor.update(0.02);
    CHECK(monitor.currentQuality() == GpuQuality::MEDIUM);
    CHECK(monitor.isDegraded());
    // Two more bursts: MEDIUM -> LOW after another 3 over-budget frames.
    for (int i = 0; i < 3; ++i) monitor.update(0.02);
    CHECK(monitor.currentQuality() == GpuQuality::LOW);
    CHECK(monitor.metrics().overloadFrames >= 6);
    // LOW is the floor: more over-budget frames stay LOW.
    for (int i = 0; i < 6; ++i) monitor.update(0.02);
    CHECK(monitor.currentQuality() == GpuQuality::LOW);
}

TEST_CASE("GpuMonitor recovers after 10 consecutive good frames") {
    GpuMonitor monitor;
    // Drive to LOW first.
    for (int i = 0; i < 6; ++i) monitor.update(0.02);
    REQUIRE(monitor.currentQuality() == GpuQuality::LOW);
    // 10 good frames (5ms each) recover one level (LOW -> MEDIUM).
    for (int i = 0; i < 10; ++i) monitor.update(0.005);
    CHECK(monitor.currentQuality() == GpuQuality::MEDIUM);
    // Another 10 good frames: MEDIUM -> HIGH.
    for (int i = 0; i < 10; ++i) monitor.update(0.005);
    CHECK(monitor.currentQuality() == GpuQuality::HIGH);
    CHECK_FALSE(monitor.isDegraded());
    CHECK(monitor.vfxEnabled());
}

TEST_CASE("GpuMonitor rolling average converges over the window") {
    GpuMonitor monitor;
    // 10 frames at 10ms each: rolling average should reach ~10ms.
    for (int i = 0; i < 10; ++i) monitor.update(0.01);
    CHECK(monitor.metrics().rollingAvgMs == doctest::Approx(10.0).epsilon(0.01));
    // Overload frames counted only for over-budget frames.
    CHECK(monitor.metrics().overloadFrames == 0);
}

TEST_CASE("GpuMonitor reset clears counters and returns to HIGH") {
    GpuMonitor monitor;
    for (int i = 0; i < 6; ++i) monitor.update(0.02);  // drive to LOW
    REQUIRE(monitor.currentQuality() == GpuQuality::LOW);
    monitor.reset();
    CHECK(monitor.currentQuality() == GpuQuality::HIGH);
    CHECK_FALSE(monitor.isDegraded());
    CHECK(monitor.metrics().frameCount == 0);
    CHECK(monitor.metrics().overloadFrames == 0);
}

TEST_CASE("GpuMonitor resolutionScale and vfxEnabled follow quality") {
    GpuMonitor monitor;
    CHECK(monitor.resolutionScale() > 0.9f);  // HIGH scale ~1.0
    CHECK(monitor.vfxEnabled());
    for (int i = 0; i < 3; ++i) monitor.update(0.02);
    REQUIRE(monitor.currentQuality() == GpuQuality::MEDIUM);
    // MEDIUM keeps vfx but lowers scale.
    CHECK(monitor.vfxEnabled());
    CHECK(monitor.resolutionScale() < 1.0f);
    for (int i = 0; i < 3; ++i) monitor.update(0.02);
    REQUIRE(monitor.currentQuality() == GpuQuality::LOW);
    CHECK_FALSE(monitor.vfxEnabled());
}
