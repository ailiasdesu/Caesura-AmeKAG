// test_async.cpp - AsyncLoader instance lifecycle and job integration tests
#include "doctest.h"
#include "job/JobSystem.h"
#include "resource/AssetManager.h"
#include "resource/AsyncLoader.h"
#include "di/BackendRegistry.h"
#include "di/api/ThreadAssert.h"
#include "mocks/NullJobSystem.h"

#include <thread>

using namespace Caesura;

namespace {

template <typename JobSystemT>
class AsyncLoaderFixture {
public:
    explicit AsyncLoaderFixture(bool startJobs = true)
        : loader(&assets) {
        detail::g_mainThreadId = std::this_thread::get_id();
        if (startJobs) jobs.init();
        BackendRegistry::instance().setJobSystem(&jobs);
        assets.init();
        loader.init();
    }

    ~AsyncLoaderFixture() {
        loader.shutdown();
        assets.shutdown();
        BackendRegistry::instance().setJobSystem(nullptr);
        jobs.shutdown();
    }

    AsyncLoaderFixture(const AsyncLoaderFixture&) = delete;
    AsyncLoaderFixture& operator=(const AsyncLoaderFixture&) = delete;

    JobSystemT jobs;
    AssetManager assets;
    AsyncLoader loader;
};

void checkJobRegistryCleared() {
    CHECK(BackendRegistry::instance().getJobSystem() == nullptr);
}

} // namespace

TEST_CASE("AsyncLoader local instances have independent lifecycle") {
    {
        AsyncLoaderFixture<NullJobSystem> infra;
        AsyncLoader other(&infra.assets);
        CHECK(infra.loader.isRunning());
        CHECK_FALSE(other.isRunning());
    }
    checkJobRegistryCleared();
}

TEST_CASE("AsyncLoader::shutdown is idempotent") {
    {
        AsyncLoaderFixture<JobSystem> infra;
        infra.loader.shutdown();
        infra.loader.shutdown();
    }
    checkJobRegistryCleared();
}

TEST_CASE("AsyncLoader::shutdown drains queued completion callbacks") {
    {
        AsyncLoaderFixture<JobSystem> infra;
        CHECK(infra.loader.enqueue("test.png", "texture") > 0);

        infra.loader.shutdown();
        infra.jobs.pollMainThreadJobs();

        CHECK_FALSE(infra.loader.poll());
        CHECK(infra.loader.pendingCount() == 0);
    }
    checkJobRegistryCleared();
}

TEST_CASE("AsyncLoader::enqueue returns positive id") {
    {
        AsyncLoaderFixture<JobSystem> infra;
        int id = infra.loader.enqueue("test.png", "texture");
        CHECK(id > 0);
        infra.loader.cancelAll();
    }
    checkJobRegistryCleared();
}

TEST_CASE("AsyncLoader::rejects path traversal") {
    {
        AsyncLoaderFixture<JobSystem> infra;
        int id = infra.loader.enqueue("../secret.png", "texture");
        CHECK(id < 0);
    }
    checkJobRegistryCleared();
}

TEST_CASE("AsyncLoader::cancelAll clears queue") {
    {
        AsyncLoaderFixture<JobSystem> infra;
        infra.loader.enqueue("a.png", "texture");
        infra.loader.enqueue("b.png", "texture");
        infra.loader.cancelAll();
        infra.jobs.waitIdle();
        infra.jobs.pollMainThreadJobs();
        infra.loader.poll();
        CHECK(infra.loader.pendingCount() == 0);
    }
    checkJobRegistryCleared();
}

TEST_CASE("AsyncLoader::poll does not crash") {
    {
        AsyncLoaderFixture<JobSystem> infra;
        infra.jobs.pollMainThreadJobs();
        bool has = infra.loader.poll();
        (void)has;
    }
    checkJobRegistryCleared();
}

TEST_CASE("AsyncLoader with NullJobSystem: enqueue + poll") {
    {
        AsyncLoaderFixture<NullJobSystem> infra;
        int id = infra.loader.enqueue("test.png", "texture");
        CHECK(id > 0);
        bool has = infra.loader.poll();
        (void)has;
    }
    checkJobRegistryCleared();
}

TEST_CASE("AsyncLoader with NullJobSystem: cancelAll") {
    {
        AsyncLoaderFixture<NullJobSystem> infra;
        infra.loader.enqueue("a.png", "texture");
        infra.loader.enqueue("b.png", "texture");
        infra.loader.cancelAll();
        infra.loader.poll();
        CHECK(infra.loader.pendingCount() == 0);
    }
    checkJobRegistryCleared();
}

TEST_CASE("AsyncLoader with NullJobSystem: rejects path traversal") {
    {
        AsyncLoaderFixture<NullJobSystem> infra;
        int id = infra.loader.enqueue("../secret.png", "texture");
        CHECK(id < 0);
    }
    checkJobRegistryCleared();
}

TEST_CASE("AsyncLoader rejects enqueue when registered job system is stopped") {
    {
        AsyncLoaderFixture<NullJobSystem> infra(false);
        CHECK(infra.loader.enqueue("test.png", "texture") < 0);
        CHECK(infra.loader.pendingCount() == 0);
    }
    checkJobRegistryCleared();
}
