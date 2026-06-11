#include "doctest.h"
#include "../src/MiniGame/NullMiniGameBackend.h"
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
