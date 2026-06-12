// test_async.cpp - AsyncLoader tests using NullJobSystem (IU-3)
#include "doctest.h"
#include "job/JobSystem.h"
#include "resource/AsyncLoader.h"
#include "entry/Engine.h"
#include "di/BackendRegistry.h"
#include "mocks/NullJobSystem.h"

using namespace Caesura;

// Real JobSystem infrastructure (for thread-safety tests)
static void initJobInfra() {
    detail::g_mainThreadId = std::this_thread::get_id();
    JobSystem::instance().init();
    AsyncLoader::instance().init();
}

static void shutdownJobInfra() {
    AsyncLoader::instance().shutdown();
    JobSystem::instance().shutdown();
}

// NullJobSystem infrastructure (for synchronous unit tests)
static NullJobSystem g_nullJob;

static void initNullJobInfra() {
    detail::g_mainThreadId = std::this_thread::get_id();
    g_nullJob.init();
    BackendRegistry::instance().setJobSystem(&g_nullJob);
    AsyncLoader::instance().init();
}

static void shutdownNullJobInfra() {
    AsyncLoader::instance().shutdown();
    g_nullJob.shutdown();
    BackendRegistry::instance().setJobSystem(nullptr);
}

// Original tests — retained for real JobSystem thread-safety validation
TEST_CASE("AsyncLoader::singleton") {
    auto& a = AsyncLoader::instance();
    auto& b = AsyncLoader::instance();
    CHECK(&a == &b);
}

TEST_CASE("AsyncLoader::shutdown is idempotent") {
    initJobInfra();
    AsyncLoader::instance().shutdown();
    AsyncLoader::instance().shutdown();
    shutdownJobInfra();
}

TEST_CASE("AsyncLoader::enqueue returns positive id") {
    initJobInfra();
    int id = AsyncLoader::instance().enqueue("test.png", "texture");
    CHECK(id > 0);
    AsyncLoader::instance().cancelAll();
    shutdownJobInfra();
}

TEST_CASE("AsyncLoader::rejects path traversal") {
    initJobInfra();
    int id = AsyncLoader::instance().enqueue("../secret.png", "texture");
    CHECK(id < 0);
    shutdownJobInfra();
}

TEST_CASE("AsyncLoader::cancelAll clears queue") {
    initJobInfra();
    auto& al = AsyncLoader::instance();
    al.enqueue("a.png", "texture");
    al.enqueue("b.png", "texture");
    al.cancelAll();
    JobSystem::instance().waitIdle();
    JobSystem::instance().pollMainThreadJobs();
    al.poll();
    CHECK(al.pendingCount() == 0);
    shutdownJobInfra();
}

TEST_CASE("AsyncLoader::poll does not crash") {
    initJobInfra();
    JobSystem::instance().pollMainThreadJobs();
    bool has = AsyncLoader::instance().poll();
    (void)has;
    shutdownJobInfra();
}

// NullJobSystem variants — synchronous, no threads, faster
TEST_CASE("AsyncLoader with NullJobSystem: enqueue + poll") {
    initNullJobInfra();
    int id = AsyncLoader::instance().enqueue("test.png", "texture");
    CHECK(id > 0);
    bool has = AsyncLoader::instance().poll();
    (void)has;
    shutdownNullJobInfra();
}

TEST_CASE("AsyncLoader with NullJobSystem: cancelAll") {
    initNullJobInfra();
    auto& al = AsyncLoader::instance();
    al.enqueue("a.png", "texture");
    al.enqueue("b.png", "texture");
    al.cancelAll();
    al.poll();
    CHECK(al.pendingCount() == 0);
    shutdownNullJobInfra();
}

TEST_CASE("AsyncLoader with NullJobSystem: rejects path traversal") {
    initNullJobInfra();
    int id = AsyncLoader::instance().enqueue("../secret.png", "texture");
    CHECK(id < 0);
    shutdownNullJobInfra();
}
