#include "doctest.h"
#include "input/InputRouter.h"
#include <SDL3/SDL_events.h>
#include <string>

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
    CHECK(std::string(inputFocusToString(InputFocus::KAG)) == "KAG");
    CHECK(std::string(inputFocusToString(InputFocus::GAME)) == "GAME");
    CHECK(std::string(inputFocusToString(static_cast<InputFocus>(99))) == "UNKNOWN");
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

TEST_CASE("InputRouter rejects invalid focus without changing routing state") {
    InputRouter router;
    int focusChanges = 0;
    router.registerFocusChangeCallback([&](InputFocus) { ++focusChanges; });

    SDL_Event event = {};
    event.type = SDL_EVENT_KEY_DOWN;
    router.processEvent(event);
    REQUIRE(router.hasKAGClick());

    router.setFocus(static_cast<InputFocus>(99));

    CHECK(router.getFocus() == InputFocus::KAG);
    CHECK(router.hasKAGClick());
    CHECK(focusChanges == 0);
}

TEST_CASE("InputRouter routes keyboard and mouse events to exactly one focus") {
    InputRouter router;
    int kagEvents = 0;
    int gameEvents = 0;
    router.registerKAGCallback([&](const SDL_Event&) { ++kagEvents; });
    router.registerGameCallback([&](const SDL_Event&) { ++gameEvents; });

    SDL_Event key = {};
    key.type = SDL_EVENT_KEY_DOWN;
    router.processEvent(key);
    CHECK(kagEvents == 1);
    CHECK(gameEvents == 0);

    router.setFocus(InputFocus::GAME);
    CHECK_FALSE(router.hasKAGClick());

    SDL_Event mouse = {};
    mouse.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    router.processEvent(mouse);
    CHECK(kagEvents == 1);
    CHECK(gameEvents == 1);
    CHECK_FALSE(router.hasKAGClick());
}

TEST_CASE("InputRouter lets a KAG handler consume the current input") {
    InputRouter router;
    router.registerKAGCallback([&](const SDL_Event&) { router.consumeKAGClick(); });

    SDL_Event event = {};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    router.processEvent(event);

    CHECK_FALSE(router.hasKAGClick());
}

TEST_CASE("InputRouter stops the old callback chain after a focus change") {
    SUBCASE("KAG handler switches to GAME") {
        InputRouter router;
        int staleKagCalls = 0;
        int gameCalls = 0;
        router.registerKAGCallback([&](const SDL_Event&) {
            router.setFocus(InputFocus::GAME);
        });
        router.registerKAGCallback([&](const SDL_Event&) { ++staleKagCalls; });
        router.registerGameCallback([&](const SDL_Event&) { ++gameCalls; });

        SDL_Event event = {};
        event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        router.processEvent(event);

        CHECK(router.getFocus() == InputFocus::GAME);
        CHECK(staleKagCalls == 0);
        CHECK(gameCalls == 0);
        CHECK_FALSE(router.hasKAGClick());
    }

    SUBCASE("GAME handler switches to KAG") {
        InputRouter router;
        router.setFocus(InputFocus::GAME);
        int staleGameCalls = 0;
        int kagCalls = 0;
        router.registerGameCallback([&](const SDL_Event&) {
            router.setFocus(InputFocus::KAG);
        });
        router.registerGameCallback([&](const SDL_Event&) { ++staleGameCalls; });
        router.registerKAGCallback([&](const SDL_Event&) { ++kagCalls; });

        SDL_Event event = {};
        event.type = SDL_EVENT_KEY_DOWN;
        router.processEvent(event);

        CHECK(router.getFocus() == InputFocus::KAG);
        CHECK(staleGameCalls == 0);
        CHECK(kagCalls == 0);
        CHECK_FALSE(router.hasKAGClick());
    }
}
