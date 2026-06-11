#include "doctest.h"
#include "input/InputRouter.h"
#include <SDL3/SDL_events.h>

using namespace Caesura;

TEST_CASE("InputRouter::singleton lifecycle") {
    InputRouter router;
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
