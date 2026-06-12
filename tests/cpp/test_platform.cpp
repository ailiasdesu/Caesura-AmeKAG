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
