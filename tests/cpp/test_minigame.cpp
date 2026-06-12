// test_minigame.cpp - MiniGame module interface tests (F2)
#include "doctest.h"
#include "minigame/api/IMiniGameBackend.h"
#include "minigame/NullMiniGameBackend.h"
#include "di/BackendRegistry.h"
#include <cstring>

using namespace Caesura;

TEST_CASE("MiniGame: IMiniGameBackend interface upcast") {
    NullMiniGameBackend backend;
    IMiniGameBackend* iface = &backend;
    CHECK(iface != nullptr);
    CHECK(iface->getBackendName() != nullptr);
}

TEST_CASE("MiniGame: NullMiniGameBackend name is non-empty") {
    NullMiniGameBackend backend;
    CHECK(backend.getBackendName() != nullptr);
    CHECK(std::strlen(backend.getBackendName()) > 0);
}

TEST_CASE("MiniGame: NullMiniGameBackend init succeeds") {
    NullMiniGameBackend backend;
    CHECK(backend.init() == true);
}

TEST_CASE("MiniGame: NullMiniGameBackend shutdown after init") {
    NullMiniGameBackend backend;
    backend.init();
    backend.shutdown();
    CHECK(true);
}

TEST_CASE("MiniGame: NullMiniGameBackend double shutdown is safe") {
    NullMiniGameBackend backend;
    backend.init();
    backend.shutdown();
    backend.shutdown();
    CHECK(true);
}

TEST_CASE("MiniGame: NullMiniGameBackend render does not crash") {
    NullMiniGameBackend backend;
    backend.init();
    backend.render();
    CHECK(true);
}

TEST_CASE("MiniGame: NullMiniGameBackend processEvent returns false") {
    NullMiniGameBackend backend;
    CHECK(backend.processEvent(nullptr) == false);
}

TEST_CASE("MiniGame: BackendRegistry MiniGame round-trip") {
    auto& reg = BackendRegistry::instance();
    NullMiniGameBackend backend;
    reg.setMiniGameBackend(&backend);
    CHECK(reg.getMiniGameBackend() == &backend);
    reg.setMiniGameBackend(nullptr);
    CHECK(reg.getMiniGameBackend() == nullptr);
}
