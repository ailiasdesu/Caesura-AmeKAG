#include "doctest.h"
#include "job/JobSystem.h"
#include "entry/Engine.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

using namespace Caesura;

TEST_CASE("JobSystem::singleton and lifecycle") {
    auto& js = JobSystem::instance();
    js.init();
    CHECK(js.isRunning());
    CHECK(js.workerCount() >= 1);
    js.shutdown();
}

// =============================================================================
// Expanded: priority and pending jobs
// =============================================================================

TEST_CASE("JobSystem::pendingJobs tracks active work") {
    auto& js = JobSystem::instance();
    js.init();
    CHECK(js.pendingJobs() == 0);

    std::atomic<bool> done{false};

    // Allow jobs to start, then check pending count
    std::atomic<int> startCount{0};
    js.submit([&]() { startCount.fetch_add(1); while (!done.load()) {} });
    js.submit([&]() { startCount.fetch_add(1); while (!done.load()) {} });

    // Wait for both jobs to actually start before checking pending
    for (int i = 0; i < 100 && startCount.load() < 2; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    CHECK(startCount.load() == 2);  // both jobs started
    CHECK(js.pendingJobs() > 0);

    done.store(true);
    js.waitIdle();
    CHECK(js.pendingJobs() == 0);
    js.shutdown();
}

TEST_CASE("JobSystem::both priority levels execute correctly") {
    auto& js = JobSystem::instance();
    js.init();

    std::atomic<bool> normalRan{false}, lowRan{false};
    js.submit([&]() { lowRan.store(true); }, JobPriority::Low);
    js.submit([&]() { normalRan.store(true); }, JobPriority::Normal);

    js.waitIdle();
    CHECK(normalRan.load());
    CHECK(lowRan.load());
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

TEST_CASE("JobSystem shutdown never detaches workers from owned state") {
    std::ifstream source("../../../src/job/JobSystem.cpp", std::ios::binary);
    REQUIRE(source.is_open());
    const std::string contents((std::istreambuf_iterator<char>(source)),
                               std::istreambuf_iterator<char>());
    CHECK(contents.find(".detach()") == std::string::npos);
}
