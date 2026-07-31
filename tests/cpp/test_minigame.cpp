// test_minigame.cpp - MiniGame module interface tests (F2)
#include "doctest.h"
#include "minigame/api/IMiniGameBackend.h"
#include "minigame/NullMiniGameBackend.h"
#include "minigame/BgfxMiniGameBackend.h"
#include <filesystem>
#include <fstream>
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

TEST_CASE("MiniGame: loadScene parses JSON scene descriptors") {
    BgfxMiniGameBackend backend;
    REQUIRE(backend.init());

    const std::string json =
        "{ \"name\": \"test\","
        "  \"camera\": { \"eye\": [1, 2, 3], \"at\": [0, 1, 0] },"
        "  \"lights\": { \"ambient\": [0.1, 0.2, 0.3],"
        "                \"directional\": { \"dir\": [0, -1, 0],"
        "                                   \"color\": [1, 1, 1], \"intensity\": 0.5 } },"
        "  \"objects\": ["
        "    { \"type\": \"cube\", \"pos\": [0, 0, 0], \"scale\": 1, \"color\": [1, 0, 0] },"
        "    { \"type\": \"sphere\", \"pos\": [1, 1, 0], \"scale\": 0.5, \"color\": [0, 1, 0] },"
        "    { \"type\": \"plane\", \"pos\": [0, -1, 0], \"scale\": [10, 1, 10], \"color\": [0.5, 0.5, 0.5] },"
        "    { \"type\": \"cube\", \"pos\": [2, 0, 2], \"scale\": 0.25, \"color\": [0, 0, 1], \"gravity\": true }"
        "  ] }";
    const std::string path = std::filesystem::temp_directory_path().string() + "/caesura_minigame_test.json";
    {
        std::ofstream out(path, std::ios::binary);
        out << json;
    }

    // Scene parsing is GPU-free; enter()/render() require a live bgfx
    // context and are covered by GPU smoke tests instead.
    const uint32_t handle = backend.loadScene(path);
    REQUIRE(handle != 0);
    CHECK(backend.sceneCount() == 1);

    // Loading the same scene twice yields distinct handles
    const uint32_t handle2 = backend.loadScene(path);
    REQUIRE(handle2 != 0);
    CHECK(handle2 != handle);
    CHECK(backend.sceneCount() == 2);

    // Missing file and invalid JSON fail cleanly
    CHECK(backend.loadScene(path + ".does_not_exist") == 0);
    {
        std::ofstream out(path, std::ios::binary);
        out << "{ not valid json";
    }
    CHECK(backend.loadScene(path) == 0);

    backend.unloadScene(handle);
    backend.unloadScene(handle2);
    CHECK(backend.sceneCount() == 0);

    backend.shutdown();
    std::filesystem::remove(path);
}