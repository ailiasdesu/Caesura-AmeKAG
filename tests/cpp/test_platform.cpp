// test_platform.cpp - platform module unit tests (S2.3)
#include "doctest.h"
#include "platform/SDL3PlatformBackend.h"
#include "platform/api/IPlatformBackend.h"
#include "platform/SDL3DisplayService.h"
#include "platform/NullDisplayService.h"
#include "platform/api/IDisplayService.h"
#include "platform/MobileAdapter.h"
#include "platform/NullPlatformBackend.h"
#include <cstring>

using namespace Caesura;

TEST_CASE("Platform: SDL3PlatformBackend default construction") {
    CHECK_NOTHROW(SDL3PlatformBackend());
}

TEST_CASE("Platform: SDL3PlatformBackend name is non-empty") {
    SDL3PlatformBackend backend;
    CHECK(backend.getBackendName() != nullptr);
    CHECK(std::strcmp(backend.getBackendName(), "SDL3") == 0);
}

TEST_CASE("Platform: SDL3PlatformBackend default window size") {
    SDL3PlatformBackend backend;
    CHECK(backend.getWindowWidth() == 1280);
    CHECK(backend.getWindowHeight() == 720);
}

TEST_CASE("Platform: SDL3PlatformBackend pollEvent before init is safe") {
    SDL3PlatformBackend backend;
    CHECK_NOTHROW(backend.pollEvent());
}

TEST_CASE("Platform: IPlatformBackend interface upcast") {
    SDL3PlatformBackend backend;
    IPlatformBackend* iface = &backend;
    CHECK(iface != nullptr);
    CHECK(iface->getBackendName() != nullptr);
}

// =============================================================================
// Expanded: pre-init safety checks
// =============================================================================

TEST_CASE("Platform: SDL3PlatformBackend getTicksMs before init") {
    SDL3PlatformBackend backend;
    // SDL_GetTicks() works without an active window.
    // Verify it returns a positive value or zero (not a sentinel/UINT64_MAX).
    uint64_t t = backend.getTicksMs();
    CHECK(t < UINT64_MAX);  // not overflow sentinel
}

TEST_CASE("Platform: SDL3PlatformBackend getMouseState before init") {
    SDL3PlatformBackend backend;
    auto mouse = backend.getMouseState();
    CHECK(mouse.x == 0);
    CHECK(mouse.y == 0);
    CHECK_FALSE(mouse.leftDown);
}

TEST_CASE("Platform: SDL3PlatformBackend getNativeWindowHandle before init") {
    SDL3PlatformBackend backend;
    // Native window handle should be nullptr before SDL init
    CHECK(backend.getNativeWindowHandle() == nullptr);
}

TEST_CASE("Platform: SDL3PlatformBackend setFullscreen before init is safe") {
    SDL3PlatformBackend backend;
    CHECK_NOTHROW(backend.setFullscreen(true));
    CHECK_NOTHROW(backend.setFullscreen(false));
}

TEST_CASE("Platform: SDL3PlatformBackend resizeWindow before init is safe") {
    SDL3PlatformBackend backend;
    CHECK_NOTHROW(backend.resizeWindow(1920, 1080));
}

TEST_CASE("Platform: SDL3PlatformBackend shutdown before init is safe") {
    SDL3PlatformBackend backend;
    CHECK_NOTHROW(backend.shutdown());
    CHECK_NOTHROW(backend.shutdown());  // idempotent
}
// =============================================================================
// G11: platform layer boundary tests (no-GPU / no-window safe)
// =============================================================================

TEST_CASE("Platform: getTicksMs is a monotonic advancing time source") {
    // (c) Timing -- the runner computes frame delta as getTicksMs() deltas and
    // clamps it to [0, 0.25]s. The platform contract requires an ever-advancing,
    // never-decreasing tick source so the delta (now - last) can't go negative.
    SDL3PlatformBackend backend;
    const int samples = 12;
    uint64_t first = backend.getTicksMs();
    uint64_t prev = first;
    for (int i = 0; i < samples; ++i) {
        SDL_Delay(5);
        uint64_t cur = backend.getTicksMs();
        CHECK(cur >= prev);          // monotonic non-decreasing
        CHECK(cur < UINT64_MAX);     // not an overflow sentinel
        prev = cur;
    }
    // The tick source actually advances: sleeping over the samples must move
    // the last reading strictly past the first (positive frame delta).
    CHECK(prev > first);
}

TEST_CASE("Platform: NullPlatformBackend resize maps to window dimensions") {
    // (d) resizeWindow -> getWindowWidth/Height mapping (no-GPU backend that
    // models the IPlatformBackend contract without creating a real window).
    NullPlatformBackend backend;
    CHECK(backend.getWindowWidth() == 0);
    CHECK(backend.getWindowHeight() == 0);

    CHECK(backend.init("test", 1920, 1080));
    CHECK(backend.getWindowWidth() == 1920);
    CHECK(backend.getWindowHeight() == 1080);

    backend.resizeWindow(800, 600);
    CHECK(backend.getWindowWidth() == 800);
    CHECK(backend.getWindowHeight() == 600);

    backend.shutdown();
}

TEST_CASE("Platform: NullPlatformBackend interface upcast identity") {
    // The concrete backend must satisfy the documented IPlatformBackend calls
    // safely with no window and no GPU.
    NullPlatformBackend backend;
    IPlatformBackend* iface = &backend;
    CHECK(iface->getBackendName() != nullptr);
    CHECK(std::strcmp(iface->getBackendName(), "NullPlatform") == 0);
    CHECK(iface->pollEvent() == false);
    CHECK(iface->getNativeWindowHandle() == nullptr);
    IPlatformBackend::MouseState ms = iface->getMouseState();
    CHECK(ms.x == 0);
    CHECK(ms.y == 0);
    CHECK_FALSE(ms.leftDown);
}


// =============================================================================
// round 79: platform lifecycle, resize-sequence, timing, window edges
// =============================================================================

TEST_CASE("Platform: NullPlatformBackend resize sequence tracks dimensions") {
    // Window management edge: a sequence of resize calls must never leave the
    // backend with a stale dimension -- every resize updates width and height.
    NullPlatformBackend backend;
    backend.init("t", 100, 100);
    const int pairs[][2] = {
        { 800, 600 }, { 1920, 1080 }, { 320, 240 }, { 1280, 720 }
    };
    for (const auto& p : pairs) {
        backend.resizeWindow(p[0], p[1]);
        CHECK(backend.getWindowWidth() == p[0]);
        CHECK(backend.getWindowHeight() == p[1]);
    }
    backend.shutdown();
}

TEST_CASE("Platform: NullPlatformBackend init is idempotent over repeated calls") {
    // Lifecycle edge: calling init twice must not break the dimensions set by
    // the first call; the most recent init wins and is reflected immediately.
    NullPlatformBackend backend;
    CHECK(backend.init("a", 640, 480));
    CHECK(backend.getWindowWidth() == 640);
    CHECK(backend.getWindowHeight() == 480);

    CHECK(backend.init("b", 1024, 768));
    CHECK(backend.getWindowWidth() == 1024);
    CHECK(backend.getWindowHeight() == 768);
    backend.shutdown();
}

TEST_CASE("Platform: NullPlatformBackend resize before init is a no-op") {
    // Window-management edge, flipped by round 93: resizeWindow() is now gated
    // on m_initialized, matching SDL3PlatformBackend::resizeWindow which no-ops
    // until a real window exists. A pre-init resize must NOT alter dimensions.
    NullPlatformBackend backend;
    CHECK(backend.getWindowWidth() == 0);
    CHECK(backend.getWindowHeight() == 0);
    CHECK_NOTHROW(backend.resizeWindow(800, 600));
    CHECK(backend.getWindowWidth() == 0);
    CHECK(backend.getWindowHeight() == 0);
}

TEST_CASE("Platform: NullPlatformBackend resize after init updates dimensions") {
    // Window-management edge: once initialized, resizeWindow must take effect
    // and map through to getWindowWidth/Height.
    NullPlatformBackend backend;
    CHECK(backend.init("t", 1920, 1080));
    backend.resizeWindow(800, 600);
    CHECK(backend.getWindowWidth() == 800);
    CHECK(backend.getWindowHeight() == 600);
    backend.shutdown();
}

TEST_CASE("Platform: NullPlatformBackend resize after shutdown is a no-op") {
    // Window-management edge: after shutdown the backend is no longer
    // initialized, so a resize must be a no-op (dimensions stay frozen).
    NullPlatformBackend backend;
    CHECK(backend.init("t", 1920, 1080));
    backend.resizeWindow(800, 600);
    CHECK(backend.getWindowWidth() == 800);
    CHECK(backend.getWindowHeight() == 600);

    backend.shutdown();
    backend.resizeWindow(320, 240);
    CHECK(backend.getWindowWidth() == 800);
    CHECK(backend.getWindowHeight() == 600);
}

TEST_CASE("Platform: NullPlatformBackend fullscreen toggle does not change size") {
    // Window management: setFullscreen is a safe no-op on the null backend that
    // never silently alters the window dimensions.
    NullPlatformBackend backend;
    backend.init("t", 800, 600);
    CHECK_NOTHROW(backend.setFullscreen(true));
    CHECK(backend.getWindowWidth() == 800);
    CHECK(backend.getWindowHeight() == 600);
    CHECK_NOTHROW(backend.setFullscreen(false));
    CHECK(backend.getWindowWidth() == 800);
    CHECK(backend.getWindowHeight() == 600);
    backend.shutdown();
}

TEST_CASE("Platform: NullPlatformBackend timing is a constant sentinel") {
    // Timing contract: the null backend advertises a deterministic, non-growing
    // tick source (0) rather than pretending to be a monotonic clock -- engine
    // code that special-cases a zero baseline must not misbehave.
    NullPlatformBackend backend;
    CHECK(backend.getTicksMs() == 0);
    CHECK(backend.getTicksMs() == 0);
    backend.init("t", 100, 100);
    CHECK(backend.getTicksMs() == 0);   // invariant across lifecycle states
    backend.shutdown();
    CHECK(backend.getTicksMs() == 0);
}

TEST_CASE("Platform: SDL3 getTicksMs back-to-back reads are non-decreasing") {
    // Timing edge: two immediate, back-to-back reads must never go backwards,
    // and a longer (cross-second-scale) sleep must advance the clock -- the
    // frame-delta clamp [0, 0.25]s depends on this.
    SDL3PlatformBackend backend;
    const uint64_t t0 = backend.getTicksMs();
    const uint64_t t1 = backend.getTicksMs();
    CHECK(t1 >= t0);

    SDL_Delay(30);
    const uint64_t t2 = backend.getTicksMs();
    CHECK(t2 >= t1);
    CHECK(t2 > t0);                    // the delay advanced the clock
    CHECK(t2 < UINT64_MAX);
}

// =============================================================================
// Track P1 — Display Service (IDisplayService)
// =============================================================================

TEST_CASE("Display: NullDisplayService reports fixed metrics (default zeros)") {
    NullDisplayService svc;
    const auto m = svc.currentMetrics();
    CHECK(m.pixelWidth == 0);
    CHECK(m.pixelHeight == 0);
    CHECK(m.logicalWidth == 0);
    CHECK(m.logicalHeight == 0);
    CHECK(m.scaleFactor == 1.0);
    CHECK(m.dpi == 96.0);
    CHECK(m.orientation == Orientation::Unknown);
    CHECK(m.safeArea.left == 0.0);
    CHECK(m.safeArea.bottom == 0.0);
}

TEST_CASE("Display: NullDisplayService honors injected logical size") {
    NullDisplayService svc(1280, 720);
    const auto m = svc.currentMetrics();
    CHECK(m.logicalWidth == 1280);
    CHECK(m.logicalHeight == 720);
    CHECK(m.pixelWidth == 1280);
    CHECK(m.pixelHeight == 720);
}

TEST_CASE("Display: SDL3DisplayService with no platform backend reports zeros") {
    SDL3DisplayService svc(nullptr);
    const auto m = svc.currentMetrics();
    CHECK(m.pixelWidth == 0);
    CHECK(m.logicalHeight == 0);
    CHECK(m.orientation == Orientation::Unknown);
}

TEST_CASE("Display: SDL3DisplayService with an uninitialized platform is safe") {
    // No window yet (backend constructed, init() not called) -> zero metrics.
    SDL3PlatformBackend backend;
    SDL3DisplayService svc(&backend);
    const auto m = svc.currentMetrics();
    CHECK(m.pixelWidth == 0);
    CHECK(m.pixelHeight == 0);
    CHECK(m.orientation == Orientation::Unknown);
}

TEST_CASE("Display: IDisplayService interface upcast") {
    NullDisplayService svc(64, 32);
    IDisplayService* iface = &svc;
    REQUIRE(iface != nullptr);
    const auto m = iface->currentMetrics();
    CHECK(m.logicalWidth == 64);
}