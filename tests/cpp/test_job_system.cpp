#include "doctest.h"
#include "job/JobSystem.h"
#include "entry/Engine.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

using namespace Caesura;

TEST_CASE("JobSystem local instance lifecycle") {
    JobSystem js;
    js.init();
    CHECK(js.isRunning());
    CHECK(js.workerCount() >= 1);
    js.shutdown();
}

// =============================================================================
// Expanded: priority and pending jobs
// =============================================================================

TEST_CASE("JobSystem::pendingJobs tracks active work") {
    JobSystem js;
    js.init();
    CHECK(js.pendingJobs() == 0);

    std::atomic<bool> done{false};

    // Allow jobs to start, then check pending count
    std::atomic<int> startCount{0};
    const auto spin = [&done]() {
        while (!done.load()) std::this_thread::yield();
    };
    js.submit([&]() { startCount.fetch_add(1); spin(); });
    js.submit([&]() { startCount.fetch_add(1); spin(); });

    // With a single worker (hw < 4 systems) only one job can run at a time;
    // the concurrency assertion is only meaningful with 2+ workers.
    const bool concurrent = js.workerCount() >= 2;

    // Wait for jobs to start (both when concurrent, one otherwise)
    const int target = concurrent ? 2 : 1;
    for (int i = 0; i < 200 && startCount.load() < target; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (concurrent) {
        CHECK(startCount.load() == 2);  // both jobs started concurrently
    } else {
        CHECK(startCount.load() >= 1);
        MESSAGE("Single-worker system; skipping 2-job concurrency assertion");
    }
    CHECK(js.pendingJobs() > 0);

    done.store(true);
    js.waitIdle();
    CHECK(js.pendingJobs() == 0);
    js.shutdown();
}

TEST_CASE("JobSystem::both priority levels execute correctly") {
    JobSystem js;
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
    JobSystem js;
    js.init();

    std::atomic<bool> ran{false};
    js.submit([&js, &ran]() {
        CHECK(js.isWorkerThread());
        ran.store(true);
    });

    js.waitIdle();
    js.pollMainThreadJobs();
    CHECK(ran.load());

    js.shutdown();
}

TEST_CASE("JobSystem::main thread callback") {
    JobSystem js;
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
    JobSystem js;
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
    const auto sourcePath = std::filesystem::path(CAESURA_SOURCE_DIR) /
                            "src/job/JobSystem.cpp";
    std::ifstream source(sourcePath, std::ios::binary);
    REQUIRE(source.is_open());
    const std::string contents((std::istreambuf_iterator<char>(source)),
                               std::istreambuf_iterator<char>());
    CHECK(contents.find(".detach()") == std::string::npos);
}
