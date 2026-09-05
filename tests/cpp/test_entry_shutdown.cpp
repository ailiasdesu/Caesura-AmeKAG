#include "doctest.h"
#include "job/api/IJobSystem.h"
#include "entry/Engine.h"
#include "entry/EngineConfig.h"
#include "di/BackendRegistry.h"
#include "EntryLifecycleBackends.h"
#include "script/vm/LuaManager.h"
#include <atomic>
#include <future>
#include <chrono>
#include <thread>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

namespace {
// pendingJobs reaches zero only after workers have queued their completions.
// Do not poll here: the test controls exactly when a main-thread batch begins.
bool workersFinished(IJobSystem& jobs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (jobs.pendingJobs() != 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    return jobs.pendingJobs() == 0;
}
}

TEST_CASE("Entry U6: final job callbacks precede backend teardown") {
    Test::LifecycleProbe render;
    Test::LifecycleProbe miniGame;
    Test::ServiceProbe layers;
    int callbacks = 0;
    EngineConfig config;
    config.headless = true;
    config.render = new Test::RenderDevice(render);
    config.miniGame = new Test::MiniGameBackend(miniGame);
    config.layerManager = new Test::LayerManagerBackend(layers);
    Engine engine(std::move(config));
    REQUIRE(engine.init());
    auto& jobs = engine.jobSystem();
    REQUIRE(jobs.submit([] {}, JobPriority::Normal, [&] {
        ++callbacks;
        CHECK(render.beginShutdownCalls == 0);
        CHECK(layers.shutdownCalls == 0);
        CHECK(miniGame.shutdownCalls == 0);
        CHECK(BackendRegistry::instance().getLayerManager() != nullptr);
        CHECK(BackendRegistry::instance().getMiniGameBackend() != nullptr);
        CHECK_FALSE(jobs.isRunning());
        CHECK(jobs.workerCount() == 0);
        CHECK(jobs.submit([] {}) == 0);
    }) > 0);
    REQUIRE(workersFinished(jobs));
    engine.shutdown();
    CHECK(callbacks == 1);
    CHECK(render.beginShutdownCalls == 1);
    CHECK(layers.shutdownCalls == 1);
    CHECK(miniGame.shutdownCalls == 1);
    CHECK(BackendRegistry::instance().getJobSystem() == nullptr);
}

TEST_CASE("Entry U6: shutdown from the owner pump ends the current frame") {
    bool fromCompletion = false;
    SUBCASE("owner pump") {}
    SUBCASE("job completion") { fromCompletion = true; }
    Test::LifecycleProbe audio;
    int ticks = 0;
    EngineConfig config;
    config.headless = true;
    config.frameLimit = 2;
    config.audio = new Test::AudioBackend(audio);
    Engine engine(std::move(config));
    REQUIRE(engine.init());
    if (fromCompletion) {
        REQUIRE(engine.jobSystem().submit([] {}, JobPriority::Normal,
                                           [&] { engine.shutdown(); }) > 0);
        REQUIRE(workersFinished(engine.jobSystem()));
    }
    engine.run([&] {
        ++ticks;
        if (!fromCompletion) engine.shutdown();
        if (ticks > 1) engine.quit();
    });
    CHECK(ticks == 1);
    CHECK(audio.audioUpdateCalls == 0);
    CHECK(audio.shutdownCalls == 1);
}

TEST_CASE("Entry U6: shutdown joins active work and cancels pending Lua loads") {
    std::atomic<bool> observedClosedAdmission{false};
    std::promise<void> started;
    int asyncCallbacks = 0;
    EngineConfig config;
    config.headless = true;
    Engine engine(std::move(config));
    REQUIRE(engine.init());
    auto& jobs = engine.jobSystem();
    REQUIRE(jobs.submit([&] {
        started.set_value();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (jobs.isRunning() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::yield();
        observedClosedAdmission = !jobs.isRunning();
    }) > 0);
    REQUIRE(started.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    lua_State* L = engine.lua().state();
    lua_pushlightuserdata(L, &asyncCallbacks);
    lua_pushcclosure(L, [](lua_State* state) -> int {
        ++*static_cast<int*>(lua_touserdata(state, lua_upvalueindex(1)));
        return 0;
    }, 1);
    lua_setglobal(L, "u6_async_callback");
    REQUIRE(luaL_dostring(L,
        "assert(Render.load_texture_async('__missing_u6_shutdown__.png', u6_async_callback) > 0)") == LUA_OK);
    engine.shutdown();
    CHECK(observedClosedAdmission.load());
    CHECK(jobs.pendingJobs() == 0);
    CHECK(jobs.workerCount() == 0);
    CHECK(asyncCallbacks == 0);
}
