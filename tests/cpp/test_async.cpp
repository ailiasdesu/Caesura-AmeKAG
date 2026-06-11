#include "doctest.h"
#include "Core/JobSystem.h"
#include "Resource/AsyncLoader.h"
#include "Core/Engine.h"

using namespace Caesura;

static void initJobInfra() {
    Engine::s_mainThreadId = std::this_thread::get_id();
    JobSystem::instance().init();
    AsyncLoader::instance().init();
}

static void shutdownJobInfra() {
    AsyncLoader::instance().shutdown();
    JobSystem::instance().shutdown();
}

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
