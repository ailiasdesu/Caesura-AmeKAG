#include "doctest.h"
#include "input/InputRouter.h"
#include <SDL3/SDL_events.h>

using namespace Caesura;

TEST_CASE("InputRouter::default focus is KAG") {
    InputRouter router;  // NOT a singleton — each test creates its own instance
    CHECK(router.getFocus() == InputFocus::KAG);
}

TEST_CASE("InputRouter::focus switching") {
    InputRouter router;
    router.setFocus(InputFocus::GAME);
    CHECK(router.getFocus() == InputFocus::GAME);
    router.setFocus(InputFocus::KAG);
    CHECK(router.getFocus() == InputFocus::KAG);
}

TEST_CASE("InputRouter::focus name query") {
    const char* n1 = inputFocusToString(InputFocus::KAG);
    const char* n2 = inputFocusToString(InputFocus::GAME);
    CHECK(n1 != nullptr);
    CHECK(n2 != nullptr);
}

TEST_CASE("InputRouter::processEvent does not crash") {
    InputRouter router;
    SDL_Event evt = {};
    evt.type = SDL_EVENT_KEY_DOWN;
    evt.key.key = SDLK_SPACE;
    router.processEvent(evt);
}

// =============================================================================
// Expanded: callback registration and event propagation
// =============================================================================

TEST_CASE("InputRouter::registerKAGCallback + hasKAGClick") {
    InputRouter router;
    bool wasCalled = false;
    router.registerKAGCallback([&](const SDL_Event&) { wasCalled = true; });
    CHECK(router.getKAGCallbackCount() == 1);

    SDL_Event evt = {};
    evt.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    router.processEvent(evt);
    CHECK(wasCalled);
    CHECK(router.isClickPending());
    CHECK(router.hasKAGClick());

    router.consumeKAGClick();
    CHECK_FALSE(router.isClickPending());
}

TEST_CASE("InputRouter::registerGameCallback invoked on GAME focus") {
    InputRouter router;
    router.setFocus(InputFocus::GAME);
    bool gameCalled = false, kagCalled = false;
    router.registerGameCallback([&](const SDL_Event&) { gameCalled = true; });
    router.registerKAGCallback([&](const SDL_Event&) { kagCalled = true; });
    CHECK(router.getGameCallbackCount() == 1);

    SDL_Event evt = {};
    evt.type = SDL_EVENT_KEY_DOWN;
    evt.key.key = SDLK_SPACE;
    router.processEvent(evt);
    CHECK(gameCalled);
    CHECK_FALSE(kagCalled);  // KAG callback not invoked when focus is GAME
}

TEST_CASE("InputRouter::registerFocusChangeCallback fires on focus switch") {
    InputRouter router;
    InputFocus captured = InputFocus::KAG;
    router.registerFocusChangeCallback([&](InputFocus f) { captured = f; });
    router.setFocus(InputFocus::GAME);
    CHECK(captured == InputFocus::GAME);
}

TEST_CASE("InputRouter::registerResizeCallback + notifyResize") {
    InputRouter router;
    int w = 0, h = 0;
    router.registerResizeCallback([&](int nw, int nh) { w = nw; h = nh; });
    router.notifyResize(800, 600);
    CHECK(w == 800);
    CHECK(h == 600);
}
