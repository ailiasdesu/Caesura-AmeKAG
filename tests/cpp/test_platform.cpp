// test_platform.cpp - platform module unit tests (S2.3)
#include "doctest.h"
#include "platform/SDL3PlatformBackend.h"
#include "platform/api/IPlatformBackend.h"
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

