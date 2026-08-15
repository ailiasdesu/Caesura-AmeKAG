#include "doctest.h"
#include "input/InputRouter.h"
#include <SDL3/SDL_events.h>
#include <string>
#include <vector>

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
// =============================================================================
// G11: platform/input layer boundary tests
// =============================================================================

TEST_CASE("InputRouter::routes all event kinds to exactly one focus") {
    // (a) focus routing -- every event reaches only the active consumer,
    // regardless of event type (mouse, keyboard).
    SUBCASE("KAG focus receives click/key; GAME gets nothing") {
        InputRouter router;
        int kag = 0, game = 0;
        router.registerKAGCallback([&](const SDL_Event&) { ++kag; });
        router.registerGameCallback([&](const SDL_Event&) { ++game; });

        // The KAG consumer only advances on click/key presses; motion and
        // other non-advancing event kinds are intentionally filtered out in
        // KAG focus (mouse motion never reaches the KAG story-advance chain).
        SDL_Event key = {};
        key.type = SDL_EVENT_KEY_DOWN;
        SDL_Event mouse = {};
        mouse.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        SDL_Event motion = {};
        motion.type = SDL_EVENT_MOUSE_MOTION;

        router.processEvent(key);
        router.processEvent(mouse);
        router.processEvent(motion);
        CHECK(kag == 2);   // key + click reach KAG; motion is filtered
        CHECK(game == 0);
    }
    SUBCASE("GAME focus receives all event kinds; KAG gets nothing") {
        InputRouter router;
        router.setFocus(InputFocus::GAME);
        int kag = 0, game = 0;
        router.registerKAGCallback([&](const SDL_Event&) { ++kag; });
        router.registerGameCallback([&](const SDL_Event&) { ++game; });

        SDL_Event key = {};
        key.type = SDL_EVENT_KEY_DOWN;
        SDL_Event mouse = {};
        mouse.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        SDL_Event motion = {};
        motion.type = SDL_EVENT_MOUSE_MOTION;

        router.processEvent(key);
        router.processEvent(mouse);
        router.processEvent(motion);
        CHECK(kag == 0);
        CHECK(game == 3);
    }
}

TEST_CASE("InputRouter::input priority resolves to the active consumer") {
    // (b) Priority: when both consumers register multiple handlers, a batch of
    // simultaneous events is delivered in registration order to the ACTIVE
    // consumer's list and never to the inactive one.
    InputRouter router;
    router.setFocus(InputFocus::GAME);
    std::vector<Uint32> order;
    router.registerGameCallback([&](const SDL_Event& e) { order.push_back(e.type); });
    router.registerGameCallback([&](const SDL_Event& e) { order.push_back(e.type); });
    router.registerKAGCallback([&](const SDL_Event& e) { order.push_back(1000 + e.type); });

    SDL_Event e1 = {}; e1.type = SDL_EVENT_KEY_DOWN;
    SDL_Event e2 = {}; e2.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    router.processEvent(e1);
    router.processEvent(e2);

    // Both GAME handlers saw both events in order; nothing leaked to KAG.
    REQUIRE(order.size() == 4);
    CHECK(order[0] == SDL_EVENT_KEY_DOWN);
    CHECK(order[1] == SDL_EVENT_KEY_DOWN);
    CHECK(order[2] == SDL_EVENT_MOUSE_BUTTON_DOWN);
    CHECK(order[3] == SDL_EVENT_MOUSE_BUTTON_DOWN);
}

TEST_CASE("InputRouter::focus switch drains stale KAG click on round-trip") {
    // (a) Focus switch must flush/suppress stale events: a pending KAG click
    // must not survive a KAG -> GAME -> KAG round-trip.
    InputRouter router;
    router.registerGameCallback([&](const SDL_Event&) {});

    SDL_Event click = {};
    click.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    router.processEvent(click);
    CHECK(router.isClickPending());

    // KAG -> GAME drains the pending click.
    router.setFocus(InputFocus::GAME);
    CHECK_FALSE(router.isClickPending());

    // GAME-mode events must not re-arm the KAG flag.
    SDL_Event key = {};
    key.type = SDL_EVENT_KEY_DOWN;
    router.processEvent(key);
    CHECK_FALSE(router.isClickPending());

    // Back to KAG with a clean slate -- no phantom click from before the switch.
    router.setFocus(InputFocus::KAG);
    CHECK_FALSE(router.isClickPending());

    // A subsequent real click re-arms the flag (clean-slate contract).
    router.processEvent(click);
    CHECK(router.isClickPending());
}

TEST_CASE("InputRouter::window resize maps to resize callbacks regardless of focus") {
    // (d) SDL_EVENT_WINDOW_RESIZED is surfaced to the engine as notifyResize;
    // all registered resize callbacks receive the new dimensions independent
    // of the current input focus, and multiple listeners all fire.
    InputRouter router;
    int aW = 0, aH = 0, bW = 0, bH = 0;
    router.registerResizeCallback([&](int w, int h) { aW = w; aH = h; });
    router.registerResizeCallback([&](int w, int h) { bW = w; bH = h; });

    SUBCASE("KAG focus") {
        router.notifyResize(1024, 768);
    }
    SUBCASE("GAME focus") {
        router.setFocus(InputFocus::GAME);
        router.notifyResize(1600, 900);
    }
    CHECK(aW == bW);
    CHECK(aH == bH);
    // Both callbacks saw the same, non-default dimensions.
    CHECK(aW > 0);
    CHECK(aH > 0);
}

TEST_CASE("InputRouter::keyboard modifier state is exposed to consumers") {
    // (e) The router passes the SDL_Keymod modifier state through unobstructed:
    // a handler can read Shift/Ctrl/Alt etc. exactly as the event carried them.
    InputRouter router;
    SDL_Keymod captured = SDL_KMOD_NONE;
    router.registerKAGCallback([&](const SDL_Event& e) { captured = e.key.mod; });

    // Combined modifier bits (left-Shift + Ctrl) must reach the KAG handler.
    SDL_Event shifted = {};
    shifted.type = SDL_EVENT_KEY_DOWN;
    shifted.key.key = SDLK_SPACE;
    shifted.key.mod = static_cast<SDL_Keymod>(SDL_KMOD_LSHIFT | SDL_KMOD_LCTRL);
    router.processEvent(shifted);
    CHECK(captured == static_cast<SDL_Keymod>(SDL_KMOD_LSHIFT | SDL_KMOD_LCTRL));

    // Unmodified key maps through as SDL_KMOD_NONE.
    captured = SDL_KMOD_NONE;
    SDL_Event plain = {};
    plain.type = SDL_EVENT_KEY_DOWN;
    plain.key.key = SDLK_SPACE;
    plain.key.mod = SDL_KMOD_NONE;
    router.processEvent(plain);
    CHECK(captured == SDL_KMOD_NONE);
}

