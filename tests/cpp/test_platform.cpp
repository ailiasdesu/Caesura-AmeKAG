// test_platform.cpp - platform module unit tests (S2.3)
#include "doctest.h"
#include "platform/SDL3PlatformBackend.h"
#include "platform/api/IPlatformBackend.h"
#include "platform/MobileAdapter.h"
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
