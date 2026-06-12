#include "doctest.h"
#include "../src/minigame/NullMiniGameBackend.h"
#include "../src/minigame/BgfxMiniGameBackend.h"
#include "../src/di/BackendRegistry.h"

using namespace Caesura;

using namespace Caesura;

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

TEST_CASE("NullMiniGameBackend::lifecycle full") {
    Caesura::NullMiniGameBackend mg;
    CHECK(mg.init() == true);
    CHECK(mg.isActive() == false);
    const char* name = mg.getBackendName(); CHECK(name != nullptr);

    uint32_t scene = mg.loadScene("test_scene.glb");
    CHECK(scene > 0);

    mg.enter(scene);
    CHECK(mg.isActive() == true);

    CHECK(mg.update(0.016f) == true);
    mg.render();

    mg.leave();
    CHECK(mg.isActive() == false);

    mg.unloadScene(scene);
    mg.shutdown();
}

TEST_CASE("NullMiniGameBackend::processEvent returns false") {
    Caesura::NullMiniGameBackend mg;
    CHECK(mg.processEvent(nullptr) == false);
}

TEST_CASE("NullMiniGameBackend::luaCall pushes result on stack") {
    lua_State* L = luaL_newstate();
    Caesura::NullMiniGameBackend mg;
    CHECK(mg.luaCall(L, "any_method") == 1);
    lua_close(L);
}

TEST_CASE("NullMiniGameBackend::idempotent leave") {
    Caesura::NullMiniGameBackend mg;
    mg.init();
    mg.leave();  // Should not crash when not active
    mg.leave();  // Double leave
    mg.shutdown();
}

TEST_CASE("BackendRegistry::setMiniGameBackend") {
    auto& reg = Caesura::BackendRegistry::instance();
    IMiniGameBackend* old = reg.getMiniGameBackend();
    CHECK(old == nullptr);  // No backend set initially
}

TEST_CASE("NullMiniGameBackend::all methods return safe values") {
    Caesura::NullMiniGameBackend mg;
    mg.init();
    CHECK(mg.getBackendName() != nullptr);
    // loadScene returns incrementing handles
    uint32_t h1 = mg.loadScene("a.glb");
    uint32_t h2 = mg.loadScene("b.glb");
    CHECK(h2 > h1);
    // unloadScene is no-op
    mg.unloadScene(h1);
    // enter/leave toggles active state
    CHECK(mg.isActive() == false);
    mg.enter(h1);
    CHECK(mg.isActive() == true);
    mg.leave();
    CHECK(mg.isActive() == false);
    // update always returns true
    CHECK(mg.update(0.0f) == true);
    // processEvent always returns false
    CHECK(mg.processEvent(nullptr) == false);
    mg.shutdown();
}

TEST_CASE("BgfxMiniGameBackend::construct and shutdown without GPU") {
    // BgfxMiniGameBackend should construct safely even without bgfx init
    BgfxMiniGameBackend* mg = new BgfxMiniGameBackend();
    CHECK(mg->getBackendName() != nullptr);
    CHECK(mg->isActive() == false);
    // init returns true (GPU resources created lazily)
    CHECK(mg->init() == true);
    // shutdown should be safe (m_gpuReady is false, so it returns early)
    mg->shutdown();
    delete mg;
}

TEST_CASE("BgfxMiniGameBackend::scene load/unload without GPU") {
    BgfxMiniGameBackend mg;
    mg.init();
    uint32_t h = mg.loadScene("test.glb");
    CHECK(h > 0);
    mg.unloadScene(h);  // safe (not the active scene)
    mg.shutdown();
}

TEST_CASE("BgfxMiniGameBackend::setRenderDevice") {
    BgfxMiniGameBackend mg;
    mg.setRenderDevice(nullptr);  // should not crash
}
