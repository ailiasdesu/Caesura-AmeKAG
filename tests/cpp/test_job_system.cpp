#include "doctest.h"
#include "job/JobSystem.h"
#include "entry/Engine.h"
#include <atomic>
#include <chrono>
#include <thread>

using namespace Caesura;

TEST_CASE("JobSystem::singleton and lifecycle") {
    auto& js = JobSystem::instance();
    js.init();
    CHECK(js.isRunning());
    CHECK(js.workerCount() >= 1);
    js.shutdown();
}

TEST_CASE("JobSystem::submit runs work on worker") {
    auto& js = JobSystem::instance();
    js.init();

    std::atomic<bool> ran{false};
    js.submit([&ran]() {
        CHECK(JobSystem::instance().isWorkerThread());
        ran.store(true);
    });

    js.waitIdle();
    js.pollMainThreadJobs();
    CHECK(ran.load());

    js.shutdown();
}

TEST_CASE("JobSystem::main thread callback") {
    auto& js = JobSystem::instance();
    js.init();

    std::atomic<bool> workerDone{false};
    std::atomic<bool> mainDone{false};

    js.submit(
        [&workerDone]() { workerDone.store(true); },
        JobPriority::Normal,
        [&mainDone]() {
            CHECK(std::this_thread::get_id() == detail::g_mainThreadId);
            mainDone.store(true);
        });

    for (int i = 0; i < 200 && !mainDone.load(); ++i) {
        js.pollMainThreadJobs();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    CHECK(workerDone.load());
    CHECK(mainDone.load());

    js.shutdown();
}

TEST_CASE("JobSystem::parallel jobs") {
    auto& js = JobSystem::instance();
    js.init();

    std::atomic<int> counter{0};
    constexpr int kJobs = 8;
    for (int i = 0; i < kJobs; ++i) {
        js.submit([&counter]() { counter.fetch_add(1); });
    }

    js.waitIdle();
    CHECK(counter.load() == kJobs);

    js.shutdown();
}
