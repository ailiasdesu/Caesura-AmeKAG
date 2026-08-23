#include "doctest.h"
#include "input/InputRouter.h"
#include <SDL3/SDL_events.h>
#include <string>
#include <vector>

using namespace Caesura;

namespace {
void PointEv(IInputRouter& r, PointerAction a, float x, float y, int32_t id = 0, float scale = 1.0f) {
    PointerEvent pe;
    pe.action = a; pe.x = x; pe.y = y; pe.pointerId = id; pe.scale = scale;
    r.submitPointer(pe);
}
InputRouter& routerInstance() {
    static InputRouter router;
    return router;
}
} // namespace

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


// =============================================================================
// round 93b: scancode/key passthrough, key-repeat, modifiers+Meta/Win,
//            multi-device (keyboard/mouse/gamepad), text-input IME, lifecycle
// =============================================================================
// The InputRouter is a pure routing layer: it does NOT transform SDL events.
// A KEY_DOWN carries {scancode, key, mod, raw, repeat} verbatim to the active
// focus callback. These tests lock down that passthrough contract ("unknown
// scancode" handling == pass through unchanged) and the focus-based filtering
// rules for every input device family the engine can receive.

// ---- 1. scancode/key passthrough (letters/digits/functions/arrows/unknown) ---
TEST_CASE("InputRouter::key scancode+keycode pass through verbatim") {
    // A representative sweep across SDL physical scancodes and virtual key
    // codes (letters, digits, function keys, arrows). The router must deliver
    // the exact scancode/key pair it received -- it owns no mapping table.
    InputRouter router;
    struct Pair { SDL_Scancode sc; SDL_Keycode kc; };
    const Pair pairs[] = {
        { SDL_SCANCODE_A,         SDLK_A          },
        { SDL_SCANCODE_0,         SDLK_0          },
        { SDL_SCANCODE_F1,        SDLK_F1         },
        { SDL_SCANCODE_UP,        SDLK_UP         },
        { SDL_SCANCODE_DOWN,      SDLK_DOWN       },
        { SDL_SCANCODE_LEFT,      SDLK_LEFT       },
        { SDL_SCANCODE_RIGHT,     SDLK_RIGHT      },
        { SDL_SCANCODE_RETURN,    SDLK_RETURN     },
    };
    for (const Pair& pr : pairs) {
        SDL_Scancode gotSc = SDL_SCANCODE_UNKNOWN;
        SDL_Keycode  gotKc = SDLK_UNKNOWN;
        router.registerKAGCallback([&](const SDL_Event& e) {
            gotSc = e.key.scancode;
            gotKc = e.key.key;
        });
        router.consumeKAGClick();          // reset pending flag between iterations
        SDL_Event evt = {};
        evt.type = SDL_EVENT_KEY_DOWN;
        evt.key.scancode = pr.sc;
        evt.key.key = pr.kc;
        evt.key.repeat = false;
        router.processEvent(evt);
        CHECK(gotSc == pr.sc);
        CHECK(gotKc == pr.kc);
        CHECK(router.isClickPending());
    }
}

TEST_CASE("InputRouter::unknown scancode/keycode are passed through, not dropped") {
    // "Unknown" SDL codes (SDL_SCANCODE_UNKNOWN / SDLK_UNKNOWN, and an
    // out-of-range private scancode) must reach the handler unchanged. The
    // router has no whitelist -- this locks the current passthrough semantics.
    InputRouter router;
    SDL_Scancode gotSc = SDL_SCANCODE_UNKNOWN;
    SDL_Keycode  gotKc = SDLK_UNKNOWN;
    router.registerKAGCallback([&](const SDL_Event& e) {
        gotSc = e.key.scancode; gotKc = e.key.key;
    });

    const SDL_Scancode weirdSc = static_cast<SDL_Scancode>(999);
    const SDL_Keycode  weirdKc = static_cast<SDL_Keycode>(0x40000000u | 0x2F1u);
    SDL_Event evt = {};
    evt.type = SDL_EVENT_KEY_DOWN;
    evt.key.scancode = SDL_SCANCODE_UNKNOWN;
    evt.key.key = SDLK_UNKNOWN;
    router.processEvent(evt);
    CHECK(evt.key.scancode == gotSc);
    CHECK(evt.key.key == gotKc);

    router.consumeKAGClick();
    evt.key.scancode = weirdSc;
    evt.key.key = weirdKc;
    router.processEvent(evt);
    CHECK(gotSc == weirdSc);
    CHECK(gotKc == weirdKc);
}

TEST_CASE("InputRouter::raw platform scancode field survives routing") {
    // The platform-dependent "raw" scancode (Uint16) is part of the event too;
    // it must arrive unmodified so higher layers can inspect it.
    InputRouter router;
    Uint16 gotRaw = 0;
    router.registerKAGCallback([&](const SDL_Event& e) { gotRaw = e.key.raw; });
    SDL_Event evt = {};
    evt.type = SDL_EVENT_KEY_DOWN;
    evt.key.scancode = SDL_SCANCODE_SPACE;
    evt.key.key = SDLK_SPACE;
    evt.key.raw = 0x0039u;
    router.processEvent(evt);
    CHECK(gotRaw == 0x0039u);
}

// ---- 2. key repeat ---------------------------------------------------------
TEST_CASE("InputRouter::key repeat flag is exposed to consumers") {
    // SDL marks OS-level auto-repeat with e.key.repeat. In GAME focus the
    // router is transparent to both KEY_DOWN and KEY_UP (nothing is filtered),
    // so a consumer can see repeat==true on a repeat down and repeat==false on
    // the release. (In KAG focus KEY_UP never reaches callbacks -- that filter
    // is covered by a separate case below.)
    InputRouter router;
    router.setFocus(InputFocus::GAME);
    struct Seen { Uint32 key = 0; bool repeat = false; };
    std::vector<Seen> seen;
    router.registerGameCallback([&](const SDL_Event& e) {
        if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP)
            seen.push_back({ e.key.key, e.key.repeat });
    });

    SDL_Event down = {};
    down.type = SDL_EVENT_KEY_DOWN;
    down.key.key = SDLK_SPACE;
    down.key.repeat = true;      // OS auto-repeat
    down.key.down = true;
    router.processEvent(down);

    SDL_Event up = {};
    up.type = SDL_EVENT_KEY_UP;
    up.key.key = SDLK_SPACE;
    up.key.repeat = false;       // release never repeats
    up.key.down = false;
    router.processEvent(up);

    REQUIRE(seen.size() == 2);
    CHECK(seen[0].key == SDLK_SPACE);
    CHECK(seen[0].repeat == true);
    CHECK(seen[1].key == SDLK_SPACE);
    CHECK(seen[1].repeat == false);
}

TEST_CASE("InputRouter::repeat and first-down are distinct events") {
    // The router must NOT collapse a repeated down into the original down:
    // the handler sees one callback per KEY_DOWN, in submission order, and the
    // repeat flag lets it distinguish hold-over-repeat from the initial press.
    InputRouter router;
    std::vector<bool> repeatSeq;
    router.registerKAGCallback([&](const SDL_Event& e) {
        if (e.type == SDL_EVENT_KEY_DOWN) repeatSeq.push_back(e.key.repeat);
    });

    for (int i = 0; i < 3; ++i) {
        SDL_Event evt = {};
        evt.type = SDL_EVENT_KEY_DOWN;
        evt.key.key = SDLK_A;
        evt.key.repeat = (i > 0); // first is a real press, rest are repeats
        evt.key.down = true;
        router.processEvent(evt);
    }
    REQUIRE(repeatSeq.size() == 3);
    CHECK(repeatSeq[0] == false);
    CHECK(repeatSeq[1] == true);
    CHECK(repeatSeq[2] == true);
    // Every KEY_DOWN -- repeat included -- advances the KAG story loop flag.
    CHECK(router.isClickPending());
}

TEST_CASE("InputRouter::key-up never re-arms the KAG advance flag") {
    // A release (KEY_UP) is not an advancing event: it must not set the
    // pending flag in KAG focus, and a repeat-flagged KEY_DOWN still does.
    InputRouter router;
    router.registerKAGCallback([&](const SDL_Event&) {});
    router.consumeKAGClick();
    CHECK_FALSE(router.isClickPending());

    SDL_Event up = {};
    up.type = SDL_EVENT_KEY_UP;
    up.key.key = SDLK_SPACE;
    up.key.repeat = false;
    router.processEvent(up);
    CHECK_FALSE(router.isClickPending());   // release filtered -- no advance

    SDL_Event down = {};
    down.type = SDL_EVENT_KEY_DOWN;
    down.key.key = SDLK_SPACE;
    down.key.repeat = true;
    router.processEvent(down);
    CHECK(router.isClickPending());         // press (even a repeat) advances
}

// ---- 3. modifiers, incl. Meta/Win bare modifier ----------------------------
TEST_CASE("InputRouter::combined Ctrl+Alt+Shift+Meta modifier mask passes through") {
    // All four modifier families together must arrive with every bit intact.
    InputRouter router;
    SDL_Keymod captured = SDL_KMOD_NONE;
    router.registerKAGCallback([&](const SDL_Event& e) { captured = e.key.mod; });

    const SDL_Keymod combo = static_cast<SDL_Keymod>(
        SDL_KMOD_LCTRL | SDL_KMOD_LALT | SDL_KMOD_LSHIFT | SDL_KMOD_LGUI);
    SDL_Event evt = {};
    evt.type = SDL_EVENT_KEY_DOWN;
    evt.key.key = SDLK_X;
    evt.key.mod = combo;
    router.processEvent(evt);
    CHECK(captured == combo);
}

TEST_CASE("InputRouter::Meta/Win key routes as a bare modifier KEY_DOWN") {
    // A bare Win/Meta key (no main key) is still a KEY_DOWN: it reaches the
    // KAG advance chain like Ctrl does, carrying the GUI/CMD modifier on the
    // event. Round 93 covered Ctrl; this locks down the same for Meta/Win.
    InputRouter router;
    struct Capture { SDL_Keymod mod = SDL_KMOD_NONE; SDL_Keycode key = SDLK_UNKNOWN; };
    std::vector<Capture> calls;
    router.registerKAGCallback([&](const SDL_Event& e) {
        calls.push_back({ e.key.mod, e.key.key });
    });

    SDL_Event metaDown = {};
    metaDown.type = SDL_EVENT_KEY_DOWN;
    metaDown.key.key = SDLK_LMETA;
    metaDown.key.mod = SDL_KMOD_LGUI;
    router.processEvent(metaDown);

    REQUIRE(calls.size() == 1);
    CHECK(calls[0].key == SDLK_LMETA);
    CHECK(calls[0].mod == SDL_KMOD_LGUI);
    CHECK(router.isClickPending());
}

TEST_CASE("InputRouter::modifier state preserved across down+up for Meta key") {
    // A Meta+<key> chord produces two events (KEY_DOWN, KEY_UP) that both
    // expose the same GUI modifier; the router must not mutate the mask.
    InputRouter router;
    router.setFocus(InputFocus::GAME);
    struct Capture { SDL_Keymod mod = SDL_KMOD_NONE; Uint32 type = 0; };
    std::vector<Capture> seen;
    router.registerGameCallback([&](const SDL_Event& e) {
        if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP)
            seen.push_back({ e.key.mod, e.type });
    });

    const SDL_Keymod guiShift = static_cast<SDL_Keymod>(SDL_KMOD_LGUI | SDL_KMOD_LSHIFT);
    SDL_Event down = {};
    down.type = SDL_EVENT_KEY_DOWN;
    down.key.key = SDLK_TAB;
    down.key.mod = guiShift;
    router.processEvent(down);

    SDL_Event up = {};
    up.type = SDL_EVENT_KEY_UP;
    up.key.key = SDLK_TAB;
    up.key.mod = guiShift;
    router.processEvent(up);

    REQUIRE(seen.size() == 2);
    CHECK(seen[0].mod == guiShift);
    CHECK(seen[1].mod == guiShift);
    CHECK_FALSE(router.isClickPending());  // GAME mode, no KAG advance flag
}

// ---- 4. multi-device coexistence (keyboard / mouse / gamepad) --------------
TEST_CASE("InputRouter::keyboard and mouse events coexist in focus routing") {
    // Keyboard and mouse are independent device families: in GAME focus both
    // reach the game consumer with no cross-talk and no KAG leak.
    InputRouter router;
    router.setFocus(InputFocus::GAME);
    std::vector<Uint32> game;
    router.registerGameCallback([&](const SDL_Event& e) { game.push_back(e.type); });

    SDL_Event key = {}; key.type = SDL_EVENT_KEY_DOWN; key.key.key = SDLK_SPACE;
    SDL_Event mdown = {}; mdown.type = SDL_EVENT_MOUSE_BUTTON_DOWN; mdown.button.button = 1;
    SDL_Event mup = {}; mup.type = SDL_EVENT_MOUSE_BUTTON_UP; mup.button.button = 1;
    router.processEvent(key);
    router.processEvent(mdown);
    router.processEvent(mup);

    REQUIRE(game.size() == 3);
    CHECK(game[0] == SDL_EVENT_KEY_DOWN);
    CHECK(game[1] == SDL_EVENT_MOUSE_BUTTON_DOWN);
    CHECK(game[2] == SDL_EVENT_MOUSE_BUTTON_UP);
    CHECK_FALSE(router.isClickPending());
}

TEST_CASE("InputRouter::gamepad button/axis events route to GAME focus") {
    // Gamepad events are a distinct device family. In GAME focus they must be
    // delivered to the game consumer; in KAG focus they are not advancing
    // events (never arm the KAG advance flag) and never leak to game.
    InputRouter router;
    router.setFocus(InputFocus::GAME);
    std::vector<Uint32> game;
    router.registerGameCallback([&](const SDL_Event& e) { game.push_back(e.type); });

    SDL_Event bdown = {};
    bdown.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    bdown.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
    bdown.gbutton.down = true;
    router.processEvent(bdown);

    SDL_Event bup = {};
    bup.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
    bup.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
    bup.gbutton.down = false;
    router.processEvent(bup);

    SDL_Event axis = {};
    axis.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    axis.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX;
    axis.gaxis.value = 32767;
    router.processEvent(axis);

    REQUIRE(game.size() == 3);
    CHECK(game[0] == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
    CHECK(game[1] == SDL_EVENT_GAMEPAD_BUTTON_UP);
    CHECK(game[2] == SDL_EVENT_GAMEPAD_AXIS_MOTION);
    CHECK_FALSE(router.isClickPending());   // gamepad never re-arms KAG flag
}

TEST_CASE("InputRouter::gamepad events are non-advancing in KAG focus") {
    // Under KAG focus, gamepad input is NOT a story-advance event: it is
    // filtered out (no callback, no pending flag), unlike a key press.
    InputRouter router;
    int kagCalls = 0;
    router.registerKAGCallback([&](const SDL_Event&) { ++kagCalls; });

    SDL_Event gp = {};
    gp.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    gp.gbutton.button = SDL_GAMEPAD_BUTTON_EAST;
    gp.gbutton.down = true;
    router.processEvent(gp);

    CHECK(kagCalls == 0);
    CHECK_FALSE(router.isClickPending());

    // A real key press still advances (the KAG chain is not blocked by the
    // preceding non-advancing gamepad event).
    SDL_Event key = {};
    key.type = SDL_EVENT_KEY_DOWN;
    key.key.key = SDLK_SPACE;
    router.processEvent(key);
    CHECK(kagCalls == 1);
    CHECK(router.isClickPending());
}

// ---- 5. text input / IME ---------------------------------------------------
TEST_CASE("InputRouter::text input reaches GAME focus with UTF-8 preserved") {
    // SDL_EVENT_TEXT_INPUT carries a UTF-8 string (e.text.text). In GAME focus
    // the router must deliver the exact bytes -- including multi-byte CJK.
    InputRouter router;
    router.setFocus(InputFocus::GAME);
    std::string got;
    router.registerGameCallback([&](const SDL_Event& e) {
        if (e.type == SDL_EVENT_TEXT_INPUT) got = e.text.text ? e.text.text : "";
    });

    // UTF-8 for "あ" (U+3042: E3 81 82), full-width four-byte U+1F31F, and ASCII.
    SDL_Event txt = {};
    txt.type = SDL_EVENT_TEXT_INPUT;
    txt.text.text = "ã";
    router.processEvent(txt);
    CHECK(got == "ã");

    txt.text.text = "abc";
    router.processEvent(txt);
    CHECK(got == "abc");

    txt.text.text = "ð";       // U+1F31F 🌟 (4-byte UTF-8)
    router.processEvent(txt);
    CHECK(got == "ð");
    CHECK_FALSE(router.isClickPending());
}

TEST_CASE("InputRouter::text input is non-advancing in KAG focus") {
    // In KAG focus, SDL_EVENT_TEXT_INPUT is not an advancing event: it is
    // filtered out entirely (never reaches KAG callbacks, never arms the flag).
    // IME composition produces text-input events without KEY_DOWN; the engine's
    // story-advance chain must not treat them as a click.
    InputRouter router;
    int kagCalls = 0;
    router.registerKAGCallback([&](const SDL_Event&) { ++kagCalls; });

    SDL_Event txt = {};
    txt.type = SDL_EVENT_TEXT_INPUT;
    txt.text.text = "ã";
    router.processEvent(txt);

    CHECK(kagCalls == 0);
    CHECK_FALSE(router.isClickPending());
}

TEST_CASE("InputRouter::keyboard events are not suppressed during text input") {
    // The router does not implement IME-composition suppression: a KEY_DOWN
    // that accompanies a text-input burst still reaches the active consumer
    // (each event type is filtered independently by focus rules).
    InputRouter router;
    int kagKeys = 0;
    int kagText = 0;
    router.registerKAGCallback([&](const SDL_Event& e) {
        if (e.type == SDL_EVENT_KEY_DOWN) ++kagKeys;
        if (e.type == SDL_EVENT_TEXT_INPUT) ++kagText;
    });

    SDL_Event txt = {};
    txt.type = SDL_EVENT_TEXT_INPUT;
    txt.text.text = "ã";
    router.processEvent(txt);                 // text: filtered in KAG

    SDL_Event enter = {};
    enter.type = SDL_EVENT_KEY_DOWN;
    enter.key.key = SDLK_RETURN;
    enter.key.repeat = false;
    router.processEvent(enter);               // key: advances in KAG

    CHECK(kagText == 0);
    CHECK(kagKeys == 1);
    CHECK(router.isClickPending());
}

// ---- 6. lifecycle: repeat init, drain, event-loop flush --------------------
TEST_CASE("InputRouter::repeat processEvent calls are safe and idempotent") {
    // Re-invoking the router (e.g. a fresh frame's event queue) with the same
    // router instance must not throw or corrupt routing state; callback counts
    // and focus are stable across repeated drains.
    InputRouter router;
    int kagCalls = 0;
    router.registerKAGCallback([&](const SDL_Event& e) {
        if (e.type == SDL_EVENT_KEY_DOWN) ++kagCalls;
    });

    SDL_Event key = {};
    key.type = SDL_EVENT_KEY_DOWN;
    key.key.key = SDLK_SPACE;

    for (int frame = 0; frame < 5; ++frame) {
        CHECK_NOTHROW(router.processEvent(key));
        router.consumeKAGClick();
    }
    CHECK(kagCalls == 5);
    CHECK(router.getKAGCallbackCount() == 1);
    CHECK(router.getFocus() == InputFocus::KAG);
    CHECK_FALSE(router.isClickPending());   // drained each iteration
}

TEST_CASE("InputRouter::event-loop drain leaves a consistent routing state") {
    // A full event batch (key down, up, motion, text, gamepad, wheel) processed
    // in sequence must leave the router in the same deterministic state each
    // cycle: KAG pending flag reflects only the last advancing event, and the
    // focus is unchanged across the drain.
    InputRouter router;
    router.registerKAGCallback([&](const SDL_Event&) {});

    const auto flushCycle = [&]() {
        SDL_Event key = {}; key.type = SDL_EVENT_KEY_DOWN; key.key.key = SDLK_SPACE;
        SDL_Event up = {};   up.type = SDL_EVENT_KEY_UP;   up.key.key = SDLK_SPACE;
        SDL_Event mov = {};  mov.type = SDL_EVENT_MOUSE_MOTION;
        SDL_Event txt = {};  txt.type = SDL_EVENT_TEXT_INPUT; txt.text.text = "x";
        SDL_Event gp = {};   gp.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        SDL_Event wh = {};   wh.type = SDL_EVENT_MOUSE_WHEEL;
        router.processEvent(key);
        router.processEvent(up);
        router.processEvent(mov);
        router.processEvent(txt);
        router.processEvent(gp);
        router.processEvent(wh);
    };

    flushCycle();
    CHECK(router.isClickPending());          // key down was advancing
    CHECK(router.getFocus() == InputFocus::KAG);   // focus unchanged

    router.consumeKAGClick();
    CHECK_FALSE(router.isClickPending());

    flushCycle();
    CHECK(router.isClickPending());
    CHECK(router.getFocus() == InputFocus::KAG);
}

TEST_CASE("InputRouter::many rapid events preserve order and pending edge") {
    // Queue behaviour on the KAG advancing window: interleaving down/up with
    // non-advancing events must not lose or reorder the flag transitions.
    InputRouter router;
    int kagCalls = 0;
    router.registerKAGCallback([&](const SDL_Event& e) {
        if (e.type == SDL_EVENT_KEY_DOWN) ++kagCalls;
    });

    SDL_Event down = {}; down.type = SDL_EVENT_KEY_DOWN; down.key.key = SDLK_A;
    SDL_Event up = {};   up.type = SDL_EVENT_KEY_UP;     up.key.key = SDLK_A;

    router.processEvent(down);
    CHECK(router.isClickPending());
    router.processEvent(up);          // up filtered -- flag stays
    CHECK(router.isClickPending());
    router.consumeKAGClick();
    router.processEvent(up);          // still no re-arm from release
    CHECK_FALSE(router.isClickPending());
    router.processEvent(down);        // next press re-arms
    CHECK(router.isClickPending());
    CHECK(kagCalls == 2);
}



// =============================================================================
// Track P3 — Unified pointer path (IInputRouter::submitPointer)
// =============================================================================

TEST_CASE("Pointer: Down/Up in KAG mode arms click pending + fires KAG callbacks") {
    InputRouter router;
    int kagCalls = 0;
    bool sawDown = false;
    router.registerKAGCallback([&](const SDL_Event& ev) {
        kagCalls++;
        if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.button == SDL_BUTTON_LEFT) sawDown = true;
    });
    PointEv(router, PointerAction::Down, 100, 80, 0);
    CHECK(router.isClickPending());
    CHECK(kagCalls == 1);
    CHECK(sawDown);
    PointEv(router, PointerAction::Up, 100, 80, 0);
    router.consumeKAGClick();
    CHECK_FALSE(router.isClickPending());
}

TEST_CASE("Pointer: GAME mode never leaks to KAG callbacks") {
    InputRouter router;
    router.setFocus(InputFocus::GAME);
    int kagCalls = 0, gameCalls = 0;
    router.registerKAGCallback([&](const SDL_Event&) { kagCalls++; });
    router.registerGameCallback([&](const SDL_Event&) { gameCalls++; });
    PointEv(router, PointerAction::Down, 10, 10, 0);
    PointEv(router, PointerAction::Move, 20, 20, 0);
    PointEv(router, PointerAction::Up, 20, 20, 0);
    CHECK(kagCalls == 0);
    CHECK(gameCalls == 3);
    CHECK_FALSE(router.isClickPending());
}

TEST_CASE("Pointer: LongPress maps to right-button press+release pair") {
    InputRouter router;
    int rights = 0;
    router.registerGameCallback([&](const SDL_Event& ev) {
        if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.button == SDL_BUTTON_RIGHT) rights++;
    });
    router.setFocus(InputFocus::GAME);
    PointEv(router, PointerAction::LongPress, 50, 60, 0);
    CHECK(rights == 1);
}

TEST_CASE("Pointer: Pinch maps to wheel delta from cumulative scale") {
    InputRouter router;
    std::vector<float> wheels;
    router.registerGameCallback([&](const SDL_Event& ev) {
        if (ev.type == SDL_EVENT_MOUSE_WHEEL) wheels.push_back(ev.wheel.y);
    });
    router.setFocus(InputFocus::GAME);
    PointEv(router, PointerAction::Pinch, 0, 0, 1.0f, 1.1f);   // baseline 1.0
    PointEv(router, PointerAction::Pinch, 0, 0, 1.2f, 1.3f);   // +0.2
    CHECK(wheels.size() == 2);
    CHECK(wheels[1] == doctest::Approx(2.0f));  // 0.2 * 10
    router.setFocus(InputFocus::KAG);            // pinch baseline resets
    PointEv(router, PointerAction::Pinch, 0, 0, 1.0f, 1.5f);   // new baseline
    CHECK(router.isClickPending() == false);     // pinch is not a KAG click
}

TEST_CASE("Pointer: interface exposes pointer abstraction") {
    PointerEvent pe;
    pe.action = PointerAction::Down;
    pe.x = 5; pe.y = 7; pe.pointerId = 3; pe.activePointers = 2;
    CHECK(pe.pointerId == 3);
    CHECK(pe.activePointers == 2);
    IInputRouter* r = &routerInstance();
    r->submitPointer(pe);   // smoke: no crash
}
