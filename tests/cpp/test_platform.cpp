// test_platform.cpp - platform module unit tests (S2.3)
#include "doctest.h"
#include "platform/SDL3PlatformBackend.h"
#include "platform/api/IPlatformBackend.h"
#include "platform/SDL3DisplayService.h"
#include "platform/NullDisplayService.h"
#include "platform/api/IDisplayService.h"
#include "platform/LifecycleService.h"
#include "platform/api/ILifecycleService.h"
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
    SDL3DisplayService svc(backend.window());
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


// =============================================================================
// Track P2 — Lifecycle Service (ILifecycleService)
// =============================================================================

namespace {
struct LifecycleProbe : ILifecycleListener {
    int events[8] = {0};
    int count = 0;
    void onLifecycleEvent(LifecycleEvent event) override {
        if (count < 8) events[count++] = static_cast<int>(event);
    }
};
}

TEST_CASE("Lifecycle: dispatch in registration order to all listeners") {
    LifecycleService svc;
    LifecycleProbe a, b;
    svc.addListener(&a);
    svc.addListener(&b);
    svc.post(LifecycleEvent::Background);
    svc.post(LifecycleEvent::Foreground);
    svc.post(LifecycleEvent::LowMemory);
    CHECK(a.count == 3);
    CHECK(b.count == 3);
    CHECK(a.events[0] == static_cast<int>(LifecycleEvent::Background));
    CHECK(a.events[1] == static_cast<int>(LifecycleEvent::Foreground));
    CHECK(b.events[2] == static_cast<int>(LifecycleEvent::LowMemory));
}

TEST_CASE("Lifecycle: duplicate add is ignored; remove stops delivery") {
    LifecycleService svc;
    LifecycleProbe a;
    svc.addListener(&a);
    svc.addListener(&a);  // duplicate — ignored
    svc.post(LifecycleEvent::Pause);
    CHECK(a.count == 1);
    svc.removeListener(&a);
    svc.post(LifecycleEvent::Resume);
    CHECK(a.count == 1);
    svc.removeListener(&a);  // double remove — safe no-op
}

TEST_CASE("Lifecycle: null listener and empty hub are safe") {
    LifecycleService svc;
    svc.addListener(nullptr);
    CHECK_NOTHROW(svc.post(LifecycleEvent::Terminate));
    svc.removeListener(nullptr);
    CHECK_NOTHROW(svc.post(LifecycleEvent::Background));
}

TEST_CASE("Lifecycle: ILifecycleService interface upcast") {
    LifecycleService svc;
    ILifecycleService* iface = &svc;
    REQUIRE(iface != nullptr);
    LifecycleProbe a;
    iface->addListener(&a);
    iface->post(LifecycleEvent::Foreground);
    CHECK(a.count == 1);
    iface->removeListener(&a);
}

// =============================================================================
// Track IME — Text Input / Virtual Keyboard
// =============================================================================

TEST_CASE("Platform: NullPlatformBackend text input lifecycle") {
    NullPlatformBackend backend;
    CHECK_FALSE(backend.isTextInputActive());
    CHECK_FALSE(backend.startTextInput());
    CHECK_FALSE(backend.setTextInputRect(10, 20, 100, 40, 0));

    CHECK(backend.init("test", 1280, 720));
    CHECK_FALSE(backend.isTextInputActive());

    CHECK(backend.startTextInput());
    CHECK(backend.isTextInputActive());

    CHECK(backend.setTextInputRect(100, 200, 300, 50, 5));
    CHECK(backend.isTextInputActive());

    CHECK(backend.stopTextInput());
    CHECK_FALSE(backend.isTextInputActive());

    backend.shutdown();
    CHECK_FALSE(backend.isTextInputActive());
}

TEST_CASE("Platform: SDL3PlatformBackend text input pre-init safety") {
    SDL3PlatformBackend backend;
    CHECK_FALSE(backend.isTextInputActive());
    CHECK_FALSE(backend.startTextInput());
    CHECK_FALSE(backend.stopTextInput());
    CHECK_FALSE(backend.setTextInputRect(0, 0, 100, 100));
}

TEST_CASE("Platform: IPlatformBackend text input polymorphism") {
    NullPlatformBackend backend;
    IPlatformBackend* iface = &backend;
    CHECK(iface->init("test", 1280, 720));
    CHECK(iface->startTextInput());
    CHECK(iface->isTextInputActive());
    CHECK(iface->setTextInputRect(50, 50, 200, 40, 2));
    CHECK(iface->stopTextInput());
    CHECK_FALSE(iface->isTextInputActive());
    iface->shutdown();
}

TEST_CASE("Platform Stress: rapid start/stop text input oscillation") {
    NullPlatformBackend backend;
    CHECK(backend.init("stress_test", 1280, 720));
    
    for (int i = 0; i < 10000; ++i) {
        CHECK(backend.startTextInput());
        CHECK(backend.isTextInputActive());
        CHECK(backend.stopTextInput());
        CHECK_FALSE(backend.isTextInputActive());
    }
    
    // Consecutive start calls
    for (int i = 0; i < 100; ++i) {
        CHECK(backend.startTextInput());
        CHECK(backend.isTextInputActive());
    }
    
    // Consecutive stop calls
    for (int i = 0; i < 100; ++i) {
        CHECK(backend.stopTextInput());
        CHECK_FALSE(backend.isTextInputActive());
    }
    
    backend.shutdown();
}

TEST_CASE("Platform Stress: adversarial and extreme setTextInputRect coordinates") {
    NullPlatformBackend backend;
    CHECK(backend.init("rect_stress", 1920, 1080));
    CHECK(backend.startTextInput());

    const int testCoords[][5] = {
        { -99999, -99999, -500, -100, -1 },
        { 0, 0, 0, 0, 0 },
        { 1000000, 2000000, 500000, 800000, 99999 },
        { 2147483647, 2147483647, 2147483647, 2147483647, 2147483647 },
        { -2147483647 - 1, -2147483647 - 1, -2147483647 - 1, -2147483647 - 1, -2147483647 - 1 },
        { 100, 200, 300, 40, 10 }
    };

    for (const auto& c : testCoords) {
        CHECK(backend.setTextInputRect(c[0], c[1], c[2], c[3], c[4]));
        CHECK(backend.isTextInputActive());
    }

    backend.shutdown();
}

TEST_CASE("Platform Stress: NullPlatformBackend full lifecycle idempotency and resurrection") {
    NullPlatformBackend backend;

    // Pre-init calls must safely fail
    CHECK_FALSE(backend.startTextInput());
    CHECK_FALSE(backend.setTextInputRect(10, 10, 100, 20, 0));
    CHECK_FALSE(backend.isTextInputActive());

    // Init and activate
    CHECK(backend.init("init1", 800, 600));
    CHECK(backend.startTextInput());
    CHECK(backend.isTextInputActive());

    // Shutdown resets active state
    backend.shutdown();
    CHECK_FALSE(backend.isTextInputActive());
    CHECK_FALSE(backend.startTextInput());
    CHECK_FALSE(backend.setTextInputRect(10, 10, 100, 20, 0));

    // Multiple shutdowns are safe
    backend.shutdown();
    backend.shutdown();
    CHECK_FALSE(backend.isTextInputActive());

    // Re-init resurrects backend
    CHECK(backend.init("init2", 1024, 768));
    CHECK_FALSE(backend.isTextInputActive()); // fresh init starts inactive
    CHECK(backend.startTextInput());
    CHECK(backend.isTextInputActive());
    backend.shutdown();
    CHECK_FALSE(backend.isTextInputActive());
}

TEST_CASE("Platform Stress: SDL3PlatformBackend pre-init and post-shutdown resilience") {
    SDL3PlatformBackend backend;
    
    // Multiple calls before init
    for (int i = 0; i < 50; ++i) {
        CHECK_FALSE(backend.isTextInputActive());
        CHECK_FALSE(backend.startTextInput());
        CHECK_FALSE(backend.stopTextInput());
        CHECK_FALSE(backend.setTextInputRect(i, i * 2, 100, 50, i));
    }

    // Calling shutdown before init is safe
    backend.shutdown();
    backend.shutdown();
    CHECK_FALSE(backend.isTextInputActive());
    CHECK_FALSE(backend.startTextInput());
}

// =============================================================================
// M5: MobileAdapter Gestures (TwoFingerTap, ThreeFingerHold, SwipeDown, SwipeUp)
// =============================================================================

namespace {
struct EventProbe {
    SDL_Event last{};
    int count = 0;
    static bool SDLCALL watch(void* userdata, SDL_Event* ev) {
        auto* probe = static_cast<EventProbe*>(userdata);
        probe->last = *ev;
        probe->count++;
        return true;
    }
};

struct EventProbeGuard {
    EventProbe probe;
    EventProbeGuard() { SDL_AddEventWatch(EventProbe::watch, &probe); }
    ~EventProbeGuard() { SDL_RemoveEventWatch(EventProbe::watch, &probe); }
};
} // namespace

TEST_CASE("MobileAdapter::two-finger tap injects right click pair") {
    EventProbeGuard g;
    MobileAdapter ma;
    ma.setDisplayScale(2.0f);

    ma.onTwoFingerTap(100.0f, 150.0f);
    REQUIRE(g.probe.count == 2);
    CHECK(g.probe.last.type == SDL_EVENT_MOUSE_BUTTON_UP);
    CHECK(g.probe.last.button.button == SDL_BUTTON_RIGHT);
}

TEST_CASE("MobileAdapter::three-finger hold injects skip key event") {
    EventProbeGuard g;
    MobileAdapter ma;

    ma.onThreeFingerHold(200.0f, 300.0f);
    REQUIRE(g.probe.count == 1);
    CHECK(g.probe.last.type == SDL_EVENT_KEY_DOWN);
    CHECK(g.probe.last.key.key == SDLK_LCTRL);
}

TEST_CASE("MobileAdapter::swipe down and up inject action key events") {
    EventProbeGuard g;
    MobileAdapter ma;

    ma.onSwipeDown(100.0f, 100.0f, 100.0f, 200.0f);
    REQUIRE(g.probe.count == 1);
    CHECK(g.probe.last.type == SDL_EVENT_KEY_DOWN);
    CHECK(g.probe.last.key.key == SDLK_SPACE);

    ma.onSwipeUp(100.0f, 200.0f, 100.0f, 100.0f);
    REQUIRE(g.probe.count == 2);
    CHECK(g.probe.last.type == SDL_EVENT_KEY_DOWN);
    CHECK(g.probe.last.key.key == SDLK_PAGEUP);
}

TEST_CASE("MobileAdapter::new gestures reject non-finite coordinates") {
    EventProbeGuard g;
    MobileAdapter ma;

    ma.onTwoFingerTap(NAN, 100.0f);
    ma.onThreeFingerHold(100.0f, INFINITY);
    ma.onSwipeDown(NAN, 0.0f, 0.0f, 0.0f);
    ma.onSwipeUp(0.0f, 0.0f, INFINITY, 0.0f);

    CHECK(g.probe.count == 0);
}

TEST_CASE("MobileAdapter::polymorphic dispatch via IMobileAdapter interface") {
    EventProbeGuard g;
    MobileAdapter ma;
    IMobileAdapter* iface = &ma;

    iface->onTwoFingerTap(50.0f, 50.0f);
    iface->onThreeFingerHold(50.0f, 50.0f);
    iface->onSwipeDown(0.0f, 0.0f, 0.0f, 100.0f);
    iface->onSwipeUp(0.0f, 100.0f, 0.0f, 0.0f);

    CHECK(g.probe.count == 5); // 2 from TwoFingerTap (down+up), 1 skip, 1 swipe down, 1 swipe up
}
