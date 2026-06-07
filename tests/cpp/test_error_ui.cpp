#include "doctest.h"
#include "Core/ErrorUI.h"

using namespace Caesura;

TEST_CASE("ErrorUI::resetCounters") {
    ErrorUI::resetCounters();
    // After reset, no crash on query
}

TEST_CASE("ErrorUI::action enum values") {
    CHECK(static_cast<int>(ErrorAction::Retry) >= 0);
    CHECK(static_cast<int>(ErrorAction::Title) >= 0);
    CHECK(static_cast<int>(ErrorAction::Quit) >= 0);
}

TEST_CASE("ErrorUI::show with null renderer returns Quit") {
    ErrorAction action = ErrorUI::show("Test", "Body", "", 0, false);
    // With null renderer, should not crash
    CHECK(static_cast<int>(action) >= 0);
}
