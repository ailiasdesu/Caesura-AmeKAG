#include "doctest.h"
#include "job/JobSystem.h"
#include "mocks/NullJobSystem.h"
#include <chrono>
#include <stdexcept>
#include <thread>

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

TEST_CASE("Job U6: shutdown delivers already queued completions exactly once") {
    int completions = 0;
    JobSystem jobs;
    jobs.init();
    REQUIRE(jobs.submit([] {}, JobPriority::Normal, [&] { ++completions; }) > 0);
    REQUIRE(workersFinished(jobs));
    jobs.waitIdle();
    CHECK(completions == 0);
    jobs.shutdown();
    CHECK(completions == 1);
    jobs.pollMainThreadJobs();
    jobs.shutdown();
    CHECK(completions == 1);
    CHECK(jobs.workerCount() == 0);
}

TEST_CASE("Job U6: final drain isolates exceptions and rejects reentrant admission") {
    int first = 0;
    int second = 0;
    JobSystem jobs;
    jobs.init();
    REQUIRE(jobs.submit([] {}, JobPriority::Normal, [&] {
        ++first;
        CHECK(jobs.workerCount() == 0);
        CHECK_FALSE(jobs.isRunning());
        CHECK(jobs.submit([] {}) == 0);
        jobs.init();
        CHECK_FALSE(jobs.isRunning());
        throw std::runtime_error("deliberate completion failure");
    }) > 0);
    REQUIRE(workersFinished(jobs));
    REQUIRE(jobs.submit([] {}, JobPriority::Normal, [&] { ++second; }) > 0);
    REQUIRE(workersFinished(jobs));
    CHECK_NOTHROW(jobs.shutdown());
    CHECK(first == 1);
    CHECK(second == 1);
    CHECK_FALSE(jobs.isRunning());
    CHECK(jobs.workerCount() == 0);
}

TEST_CASE("Job U6: recursive polling leaves new completions for the next batch") {
    int first = 0;
    int second = 0;
    JobSystem jobs;
    jobs.init();
    REQUIRE(jobs.submit([] {}, JobPriority::Normal, [&] {
        ++first;
        CHECK(jobs.submit([] {}, JobPriority::Normal, [&] { ++second; }) > 0);
        CHECK(workersFinished(jobs));
        jobs.waitIdle();
        jobs.pollMainThreadJobs();
        CHECK(second == 0);
    }) > 0);
    REQUIRE(workersFinished(jobs));
    jobs.pollMainThreadJobs();
    CHECK(first == 1);
    CHECK(second == 0);
    jobs.pollMainThreadJobs();
    CHECK(second == 1);
    jobs.shutdown();
}

TEST_CASE("Job U6: nested shutdown cancels the rest of the final snapshot") {
    int first = 0;
    int second = 0;
    JobSystem jobs;
    jobs.init();
    REQUIRE(jobs.submit([] {}, JobPriority::Normal, [&] {
        ++first;
        jobs.shutdown();
    }) > 0);
    REQUIRE(workersFinished(jobs));
    REQUIRE(jobs.submit([] {}, JobPriority::Normal, [&] { ++second; }) > 0);
    REQUIRE(workersFinished(jobs));
    jobs.shutdown();
    CHECK(first == 1);
    CHECK(second == 0);
    CHECK_FALSE(jobs.isRunning());
}

TEST_CASE("Job U6: shutdown inside a callback invalidates its remaining batch") {
    bool restart = false;
    SUBCASE("stop") {}
    SUBCASE("restart with a new request") { restart = true; }
    int oldCallbacks = 0;
    int freshCallbacks = 0;
    JobSystem jobs;
    jobs.init();
    REQUIRE(jobs.submit([] {}, JobPriority::Normal, [&] {
        jobs.shutdown();
        if (restart) {
            jobs.init();
            CHECK(jobs.submit([] {}, JobPriority::Normal, [&] { ++freshCallbacks; }) > 0);
        }
    }) > 0);
    REQUIRE(workersFinished(jobs));
    REQUIRE(jobs.submit([] {}, JobPriority::Normal, [&] { ++oldCallbacks; }) > 0);
    REQUIRE(workersFinished(jobs));
    jobs.pollMainThreadJobs();
    CHECK(oldCallbacks == 0);
    REQUIRE(workersFinished(jobs));
    jobs.pollMainThreadJobs();
    CHECK(freshCallbacks == (restart ? 1 : 0));
    jobs.shutdown();
}

TEST_CASE("Job U6: real and Null reject an empty work function") {
    int callbacks = 0;
    JobSystem real;
    NullJobSystem synchronous;
    real.init();
    synchronous.init();
    for (IJobSystem* jobs : {static_cast<IJobSystem*>(&real),
                            static_cast<IJobSystem*>(&synchronous)}) {
        CHECK(jobs->submit({}, JobPriority::Normal, [&] { ++callbacks; }) == 0);
        jobs->waitIdle();
        jobs->pollMainThreadJobs();
        CHECK(callbacks == 0);
        jobs->shutdown();
    }
}

TEST_CASE("Job U6: Null inline work cannot complete into a restarted owner") {
    bool restart = false;
    SUBCASE("stop during work") {}
    SUBCASE("stop and restart during work") { restart = true; }
    int callbacks = 0;
    NullJobSystem jobs;
    jobs.init();
    REQUIRE(jobs.submit([&] {
        jobs.shutdown();
        if (restart) jobs.init();
    }, JobPriority::Normal, [&] { ++callbacks; }) > 0);
    CHECK(callbacks == 0);
    if (restart) {
        REQUIRE(jobs.submit([] {}, JobPriority::Normal, [&] { ++callbacks; }) > 0);
        CHECK(callbacks == 1);
    }
    jobs.shutdown();
}
