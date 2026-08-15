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


// =============================================================================
// round 79: input routing depth, event-type matrix, modifiers, lifecycle edges
// =============================================================================

TEST_CASE("InputRouter::KAG focus filters non-advancing event types") {
    // The KAG story-advance chain only advances on KEY_DOWN and
    // MOUSE_BUTTON_DOWN. Every other SDL event type must be filtered out:
    // it neither reaches KAG callbacks NOR re-arms the click-pending flag.
    InputRouter router;
    int kagCalls = 0;
    router.registerKAGCallback([&](const SDL_Event&) { ++kagCalls; });

    struct Probe { Uint32 type; bool advancing; };
    const Probe probes[] = {
        { SDL_EVENT_KEY_DOWN,         true  },
        { SDL_EVENT_KEY_UP,           false },
        { SDL_EVENT_MOUSE_BUTTON_DOWN,true  },
        { SDL_EVENT_MOUSE_BUTTON_UP,  false },
        { SDL_EVENT_MOUSE_MOTION,     false },
        { SDL_EVENT_MOUSE_WHEEL,      false },
        { SDL_EVENT_TEXT_INPUT,       false },
        { SDL_EVENT_WINDOW_RESIZED,   false },
        { SDL_EVENT_WINDOW_CLOSE_REQUESTED, false },
        { SDL_EVENT_QUIT,             false },
    };

    for (const auto& p : probes) {
        router.consumeKAGClick();          // clear between probes
        SDL_Event evt = {};
        evt.type = p.type;
        router.processEvent(evt);
        CHECK_MESSAGE(router.isClickPending() == p.advancing,
                      "type " << p.type << " advancing?");
    }

    // Only the two advancing types ever invoked a KAG callback.
    CHECK(kagCalls == 2);
}

TEST_CASE("InputRouter::GAME focus passes every event type through") {
    // In GAME focus the router is transparent: every SDL event type is
    // delivered to game callbacks regardless of kind, with no KAG leak and
    // no click-pending re-arm.
    InputRouter router;
    router.setFocus(InputFocus::GAME);
    int kagCalls = 0;
    int gameCalls = 0;
    router.registerKAGCallback([&](const SDL_Event&) { ++kagCalls; });
    router.registerGameCallback([&](const SDL_Event&) { ++gameCalls; });

    const Uint32 types[] = {
        SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP,
        SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_MOUSE_BUTTON_UP,
        SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_WHEEL,
        SDL_EVENT_TEXT_INPUT, SDL_EVENT_WINDOW_RESIZED,
        SDL_EVENT_QUIT
    };
    for (Uint32 t : types) {
        SDL_Event evt = {};
        evt.type = t;
        router.processEvent(evt);
    }

    CHECK(gameCalls == static_cast<int>(sizeof(types) / sizeof(types[0])));
    CHECK(kagCalls == 0);                  // no leak into KAG
    CHECK_FALSE(router.isClickPending());  // GAME never re-arms KAG flag
}

TEST_CASE("InputRouter::mixed batch keeps routing order per event") {
    // Priority/dispatch: a batch of simultaneous events is processed in
    // submission order. Each event is routed to exactly one consumer based on
    // the focus state at the moment that event is processed.
    InputRouter router;
    std::vector<Uint32> order;
    router.registerGameCallback([&](const SDL_Event& e) { order.push_back(e.type); });
    router.registerKAGCallback([&](const SDL_Event& e) { order.push_back(10000 + e.type); });

    SDL_Event kd = {}; kd.type = SDL_EVENT_KEY_DOWN;       // KAG-focus: advancing
    SDL_Event mu = {}; mu.type = SDL_EVENT_MOUSE_BUTTON_UP; // KAG-focus: filtered
    SDL_Event mm = {}; mm.type = SDL_EVENT_MOUSE_MOTION;    // KAG-focus: filtered

    router.processEvent(kd);
    router.processEvent(mu);
    router.processEvent(mm);

    REQUIRE(order.size() == 1);
    CHECK(order[0] == 10000 + SDL_EVENT_KEY_DOWN); // only the advancing event hit KAG
    CHECK(router.isClickPending());
}

TEST_CASE("InputRouter::focus switch mid-batch redirects remaining events") {
    // Focus-switch race: the event currently being dispatched is committed to
    // the pre-switch consumer; any subsequent events in the same batch route
    // to the new focus. The old callback iteration is cut off immediately.
    InputRouter router;
    std::vector<Uint32> order;
    router.registerKAGCallback([&](const SDL_Event& e) {
        order.push_back(e.type);
        router.setFocus(InputFocus::GAME);   // switch happens on first KAG event
    });
    router.registerGameCallback([&](const SDL_Event& e) { order.push_back(20000 + e.type); });

    SDL_Event kd = {}; kd.type = SDL_EVENT_KEY_DOWN;        // routes to KAG (pre-switch)
    SDL_Event clk = {}; clk.type = SDL_EVENT_MOUSE_BUTTON_DOWN; // routes to GAME (post-switch)
    SDL_Event mm = {}; mm.type = SDL_EVENT_MOUSE_MOTION;       // routes to GAME (post-switch)

    router.processEvent(kd);
    router.processEvent(clk);
    router.processEvent(mm);

    REQUIRE(order.size() == 3);
    CHECK(order[0] == SDL_EVENT_KEY_DOWN);
    CHECK(order[1] == 20000 + SDL_EVENT_MOUSE_BUTTON_DOWN);
    CHECK(order[2] == 20000 + SDL_EVENT_MOUSE_MOTION);
    CHECK_FALSE(router.isClickPending());  // switch to GAME drained pending flag
}

TEST_CASE("InputRouter::modifier combination and state carry across events") {
    // Modifier state rides the event verbatim. In GAME focus, every event kind
    // passes through, so a Ctrl+Shift+<key> pair becomes two handler calls
    // (KEY_DOWN and KEY_UP) that both expose the same combined modifier bitmask
    // -- the router does not mutate or lose modifier state across events.
    InputRouter router;
    router.setFocus(InputFocus::GAME);
    struct Capture { SDL_Keymod mod = SDL_KMOD_NONE; Uint32 key = 0; };
    std::vector<Capture> seen;
    router.registerGameCallback([&](const SDL_Event& e) {
        if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP)
            seen.push_back({ e.key.mod, e.key.key });
    });

    const SDL_Keymod ctrlShift =
        static_cast<SDL_Keymod>(SDL_KMOD_LCTRL | SDL_KMOD_LSHIFT);

    SDL_Event down = {};
    down.type = SDL_EVENT_KEY_DOWN;
    down.key.key = SDLK_Z;
    down.key.mod = ctrlShift;
    router.processEvent(down);

    SDL_Event up = {};
    up.type = SDL_EVENT_KEY_UP;
    up.key.key = SDLK_Z;
    up.key.mod = ctrlShift;
    router.processEvent(up);

    REQUIRE(seen.size() == 2);
    CHECK(seen[0].key == SDLK_Z);
    CHECK(seen[0].mod == ctrlShift);
    CHECK(seen[1].key == SDLK_Z);
    CHECK(seen[1].mod == ctrlShift);   // state unchanged across the pair
    // GAME mode never re-arms the KAG advance flag.
    CHECK_FALSE(router.isClickPending());
}

TEST_CASE("InputRouter::modifier-only key reach handler with click advance") {
    // A bare modifier key (no main key) is still a KEY_DOWN: it reaches the
    // KAG advance chain in KAG focus and re-arms the pending flag.
    InputRouter router;
    int kagCalls = 0;
    SDL_Keymod lastMod = SDL_KMOD_NONE;
    router.registerKAGCallback([&](const SDL_Event& e) {
        ++kagCalls;
        lastMod = e.key.mod;
    });

    SDL_Event ctrlDown = {};
    ctrlDown.type = SDL_EVENT_KEY_DOWN;
    ctrlDown.key.key = SDLK_LCTRL;
    ctrlDown.key.mod = static_cast<SDL_Keymod>(SDL_KMOD_LCTRL);
    router.processEvent(ctrlDown);

    CHECK(kagCalls == 1);
    CHECK(lastMod == static_cast<SDL_Keymod>(SDL_KMOD_LCTRL));
    CHECK(router.isClickPending());
}

TEST_CASE("InputRouter::processEvent with no registered callbacks is safe") {
    // Lifecycle edge: dispatching any event type on a router with zero
    // callbacks must never crash and must respect the KAG filter rules.
    InputRouter router;
    const Uint32 types[] = {
        SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP, SDL_EVENT_MOUSE_BUTTON_DOWN,
        SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_WHEEL, SDL_EVENT_TEXT_INPUT,
        SDL_EVENT_WINDOW_RESIZED, SDL_EVENT_QUIT
    };
    for (Uint32 t : types) {
        SDL_Event evt = {};
        evt.type = t;
        CHECK_NOTHROW(router.processEvent(evt));
    }
    // A KEY_DOWN with no listeners still arms the advance flag (the flag is
    // set before dispatch), so the engine's story loop sees input.
    router.consumeKAGClick();
    SDL_Event kd = {};
    kd.type = SDL_EVENT_KEY_DOWN;
    router.processEvent(kd);
    CHECK(router.isClickPending());
}