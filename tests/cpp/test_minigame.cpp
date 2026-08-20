// test_minigame.cpp - MiniGame module interface tests (F2)
#include "doctest.h"
#include "minigame/api/IMiniGameBackend.h"
#include "minigame/NullMiniGameBackend.h"
#include "minigame/BgfxMiniGameBackend.h"
#include "minigame/MiniGeometry.h"
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

// ---------------------------------------------------------------------------
// Boundary lifecycle: repeated / premature / post-leave transitions (F2+F3)
// ---------------------------------------------------------------------------

TEST_CASE("MiniGame: repeated enter is idempotent and single leave fully exits") {
    NullMiniGameBackend backend;
    backend.init();
    const uint32_t s1 = backend.loadScene("a.glb");
    const uint32_t s2 = backend.loadScene("b.glb");
    REQUIRE(s1 != 0);
    REQUIRE(s2 != 0);

    backend.enter(s1);
    CHECK(backend.isActive());
    // Re-entering while already active is idempotent: it must not stack a
    // second active "depth" (a single leave() must fully deactivate).
    backend.enter(s2);
    CHECK(backend.isActive());
    backend.leave();
    CHECK_FALSE(backend.isActive());
    CHECK_FALSE(backend.isActive());  // no residual depth
    backend.shutdown();
}

TEST_CASE("MiniGame: update/render before enter are safe no-ops") {
    NullMiniGameBackend backend;
    backend.init();
    CHECK_FALSE(backend.isActive());
    // Pumping the loop on an inactive backend must not crash and must not
    // implicitly activate it.
    CHECK(backend.update(0.016f) == true);
    backend.render();
    CHECK_FALSE(backend.isActive());
    backend.shutdown();
}

TEST_CASE("MiniGame: leave-then-update stays inactive") {
    NullMiniGameBackend backend;
    backend.init();
    const uint32_t s = backend.loadScene("c.glb");
    REQUIRE(s != 0);
    backend.enter(s);
    CHECK(backend.isActive());
    backend.leave();
    CHECK_FALSE(backend.isActive());
    // update() after leave must not re-activate the backend.
    CHECK(backend.update(0.016f) == true);
    backend.render();
    CHECK_FALSE(backend.isActive());
    backend.shutdown();
}

TEST_CASE("MiniGame: enter/leave cycle is repeatable") {
    NullMiniGameBackend backend;
    backend.init();
    const uint32_t s = backend.loadScene("d.glb");
    REQUIRE(s != 0);
    backend.enter(s);
    backend.leave();
    backend.enter(s);
    CHECK(backend.isActive());
    backend.leave();
    CHECK_FALSE(backend.isActive());
    backend.shutdown();
}

TEST_CASE("MiniGame: multi-scene switching across the active loop") {
    NullMiniGameBackend backend;
    backend.init();
    const uint32_t hub = backend.loadScene("hub.glb");
    const uint32_t boss = backend.loadScene("boss.glb");
    REQUIRE(hub != 0);
    REQUIRE(boss != 0);
    REQUIRE(hub != boss);

    // Scene A loop
    backend.enter(hub);
    CHECK(backend.isActive());
    CHECK(backend.update(0.016f) == true);
    backend.render();
    backend.leave();
    CHECK_FALSE(backend.isActive());

    // Switch to scene B and drive another full frame.
    backend.enter(boss);
    CHECK(backend.isActive());
    CHECK(backend.update(0.016f) == true);
    backend.render();
    backend.leave();
    CHECK_FALSE(backend.isActive());

    backend.unloadScene(hub);
    backend.unloadScene(boss);
    backend.shutdown();
}

// ---------------------------------------------------------------------------
// Boundary state: enter argument (scene handle) passthrough via loadScene
// ---------------------------------------------------------------------------

TEST_CASE("MiniGame: loadScene returns distinct opaque handles for distinct scenes") {
    NullMiniGameBackend backend;
    backend.init();
    const uint32_t h1 = backend.loadScene("scene_one.glb");
    const uint32_t h2 = backend.loadScene("scene_two.glb");
    CHECK(h1 != 0);
    CHECK(h2 != 0);
    CHECK(h1 != h2);
    // Handles are opaque and must round-trip through enter()/leave() without
    // confusion about which scene is active.
    backend.enter(h2);
    CHECK(backend.isActive());
    backend.unloadScene(h1);
    backend.unloadScene(h2);
    backend.shutdown();
}
// =============================================================================
// RD-4: sphere geometry index stays within uint16_t range (segments capped)
// =============================================================================
TEST_CASE("MiniGame: sphere geometry caps segments to keep indices in uint16 range") {
    using namespace Caesura;
    // Default low-detail sphere stays sane.
    auto small = createSphereGeometry(8);
    CHECK_FALSE(small.vertices.empty());
    CHECK_FALSE(small.indices.empty());
    for (auto idx : small.indices) {
        CHECK(idx < small.vertices.size());
        CHECK(static_cast<uint32_t>(idx) <= 65535u);
    }

    // A huge requested segment count is clamped, not truncated.
    auto big = createSphereGeometry(100000);
    CHECK(big.vertices.size() < 65536u);   // 50882 vertices at segments=160
    for (auto idx : big.indices) {
        CHECK(idx < big.vertices.size());
    }
    // Sanity: the printed/clamped segments produced valid triangles.
    CHECK(big.indices.size() % 3 == 0);
    CHECK(big.indices.size() / 3 > 100);
}

