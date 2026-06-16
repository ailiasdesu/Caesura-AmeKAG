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
    // getTicksMs should return 0 before SDL is initialized
    // SDL_GetTicks() works even without an active window — just verify no crash
    uint64_t t = backend.getTicksMs();
    (void)t;  // value is machine-dependent
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

TEST_CASE("Platform: MobileAdapter default constructor") {
    MobileAdapter adapter;
    CHECK(adapter.isPaused() == false);
    CHECK(adapter.activeTouchCount() == 0);
    CHECK(adapter.getDisplayScale() == 1.0f);
}

TEST_CASE("Platform: MobileAdapter touch events") {
    MobileAdapter adapter;
    adapter.onFingerDown(100.0f, 200.0f, 0);
    CHECK(adapter.activeTouchCount() > 0);
    adapter.onFingerUp(100.0f, 200.0f, 0);
    CHECK(adapter.activeTouchCount() == 0);
}

TEST_CASE("Platform: MobileAdapter display scale set/get") {
    MobileAdapter adapter;
    adapter.setDisplayScale(2.5f);
    CHECK(adapter.getDisplayScale() == 2.5f);
}
