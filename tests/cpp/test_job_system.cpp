#include "doctest.h"
#include "job/JobSystem.h"
#include "di/api/ThreadAssert.h"
#include "mocks/NullJobSystem.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

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

// =============================================================================
// JOB BOUNDARY TESTS (NullJobSystem deterministic path + real JobSystem sizing)
// Verified against IJobSystem + NullJobSystem semantics as actually implemented.
// Defects found (not fixed in src/): see report.
// =============================================================================

// (a) Task completion -- a task enqueued to NullJobSystem runs to completion and
// its result + onComplete are available immediately after submit returns.
TEST_CASE("Boundary[Null] task completion: result + onComplete available immediately") {
    NullJobSystem njs;
    njs.init();
    REQUIRE(njs.isRunning());

    int result = 0;
    bool completeRan = false;
    bool onCompleteRan = false;
    uint64_t id = njs.submit(
        [&]() { result = 42; completeRan = true; },
        JobPriority::Normal,
        [&]() { onCompleteRan = true; });

    // Synchronous contract: everything already done when submit returns.
    CHECK(completeRan);
    CHECK(onCompleteRan);
    CHECK(result == 42);
    CHECK(id > 0);
    CHECK(njs.pendingJobs() == 0);
}

// (b) Ordering -- FIFO under NullJobSystem: tasks run in submission order.
TEST_CASE("Boundary[Null] ordering: enqueued tasks run in FIFO submission order") {
    NullJobSystem njs;
    njs.init();
    std::vector<int> order;
    for (int i = 1; i <= 5; ++i) {
        njs.submit([&order, i]() { order.push_back(i); });
    }
    // Synchronous submission implies FIFO order is observed trivially.
    CHECK(order == std::vector<int>({1, 2, 3, 4, 5}));
}

// (b2) Ordering across priorities under NullJobSystem: priority is ignored by the
// mock (documented), so all enqueued jobs still run in submission order.
TEST_CASE("Boundary[Null] ordering: priority values do not reorder synchronous mock") {
    NullJobSystem njs;
    njs.init();
    std::vector<int> order;
    njs.submit([&]() { order.push_back(1); }, JobPriority::High);
    njs.submit([&]() { order.push_back(2); }, JobPriority::Low);
    njs.submit([&]() { order.push_back(3); }, JobPriority::Normal);
    CHECK(order == std::vector<int>({1, 2, 3}));
}

// (c) Cancellation -- the IJobSystem API exposes NO cancellation mechanism. The only
// rejection path NullJobSystem (and the interface) provides is submit-after-shutdown,
// which never runs the task and returns a 0 (invalid) id. This is the closest
// observable to "a cancelled task never runs". Missing cancellation API is reported.
TEST_CASE("Boundary[Null] rejected task: submit after shutdown never runs and returns 0") {
    NullJobSystem njs;
    njs.init();
    njs.shutdown();  // system is down; the analog of a cancelled/rejected submission
    bool ran = false;
    uint64_t id = njs.submit([&]() { ran = true; });
    CHECK(id == 0);
    CHECK(ran == false);   // task never ran
    CHECK(njs.isRunning() == false);
    CHECK(njs.pendingJobs() == 0);
}

// (d) Exception isolation -- the null system now mirrors the real JobSystem: a
// throwing work() lambda is caught, reported, and swallowed so it can neither
// escape the submit() caller nor corrupt the queue; the onComplete callback
// still runs and submit returns a valid id.
TEST_CASE("Boundary[Null] exception isolation: throwing work is isolated, submit returns normally") {
    NullJobSystem njs;
    njs.init();
    bool onCompleteRan = false;
    uint64_t id = 0;
    REQUIRE_NOTHROW([&]() {
        id = njs.submit(
            []() { throw std::runtime_error("boom"); },
            JobPriority::Normal,
            [&]() { onCompleteRan = true; });
    }());
    CHECK(id > 0);
    CHECK(onCompleteRan);  // isolation does not drop the completion callback
    CHECK(njs.pendingJobs() == 0);
}

// (d2) After an isolated throwing job, the system stays usable: the throw does not
// propagate, the completion callback still runs, and subsequent jobs still run.
TEST_CASE("Boundary[Null] exception isolation: subsequent tasks still run after a throw") {
    NullJobSystem njs;
    njs.init();
    bool firstRan = false;
    bool firstComplete = false;
    bool laterRan = false;
    REQUIRE_NOTHROW(njs.submit(
        [&]() { firstRan = true; throw std::runtime_error("x"); },
        JobPriority::Normal,
        [&]() { firstComplete = true; }));
    CHECK(firstRan);
    CHECK(firstComplete);   // onComplete not skipped by the isolated throw
    uint64_t later = njs.submit([&]() { laterRan = true; });
    CHECK(later > 0);
    CHECK(laterRan);   // system not corrupted by the earlier throw
}

// (d3) Exception isolation for onComplete: also isolated -- a throwing completion
// callback no longer escapes submit(); later jobs still run.
TEST_CASE("Boundary[Null] exception isolation: throwing onComplete is isolated, submit returns normally") {
    NullJobSystem njs;
    njs.init();
    bool workRan = false;
    uint64_t id = 0;
    REQUIRE_NOTHROW([&]() {
        id = njs.submit(
            [&]() { workRan = true; },
            JobPriority::Normal,
            []() { throw std::runtime_error("cb"); });
    }());
    CHECK(workRan);
    CHECK(id > 0);
    // A later job still runs after the isolated callback throw.
    bool laterRan = false;
    CHECK(njs.submit([&]() { laterRan = true; }) > 0);
    CHECK(laterRan);
}

// (e) Nested / fan-out -- NullJobSystem is synchronous, so a work lambda that submits
// sub-tasks completes those sub-tasks (work + onComplete) before it returns, and the
// outer onComplete observes the fan-out already finished.
TEST_CASE("Boundary[Null] nested: sub-tasks complete before the outer task returns") {
    NullJobSystem njs;
    njs.init();
    int level2Done = 0;
    NullJobSystem* sys = &njs;
    bool outerWorkDone = false;

    njs.submit([&]() {
        // Fan out two sub-tasks from within the outer work.
        sys->submit([&]() { level2Done++; },
                    JobPriority::Normal,
                    [&]() { level2Done += 10; });
        sys->submit([&]() { level2Done++; });
        // Synchronous mock => sub-tasks already finished here.
        CHECK(level2Done == 12);
        outerWorkDone = true;
    });

    CHECK(outerWorkDone);
    CHECK(level2Done == 12);  // sub-task work (2) + sub-task onComplete (+10)

    // (e2) fan-out: outer onComplete fires only after all nested tasks finished.
    bool outerComplete = false;
    njs.submit(
        [&]() { sys->submit([&]() { /* inner */ }); },
        JobPriority::Normal,
        [&]() { outerComplete = true; });
    CHECK(outerComplete);
}

// (f) Worker-pool sizing -- the real JobSystem is constructible without a GPU window,
// so this is testable: workerCount must match computeWorkerCount() from hardware.
TEST_CASE("Boundary[JobSystem] worker-pool sizing matches available hardware") {
    JobSystem js;
    js.init();
    const unsigned hw = std::thread::hardware_concurrency();
    int expected = (hw == 0) ? 4 : ((hw < 4) ? 1 : static_cast<int>(hw) - 1);
    CHECK(js.isRunning());
    CHECK(js.workerCount() >= 1);
    CHECK(js.workerCount() == expected);
    js.shutdown();
    CHECK(js.workerCount() == 0);  // torn down cleanly
}

// (f2) Completion on the real JobSystem: waitIdle drains all submitted work.
TEST_CASE("Boundary[JobSystem] task completion via waitIdle") {
    JobSystem js;
    js.init();
    constexpr int kJobs = 16;
    std::atomic<int> counter{0};
    for (int i = 0; i < kJobs; ++i) {
        js.submit([&]() { counter.fetch_add(1); });
    }
    js.waitIdle();
    CHECK(counter.load() == kJobs);
    CHECK(js.pendingJobs() == 0);
    js.shutdown();
}

// (g) Exception isolation on the REAL JobSystem: a throwing work() -- which prior
// to isolation would escape the worker thread and terminate the process -- is
// caught, reported, and swallowed. The queue keeps draining and waitIdle does not
// wedge, so one bad task cannot kill a worker or the whole pool.
TEST_CASE("Boundary[JobSystem] exception isolation: throwing work is isolated, queue keeps draining") {
    JobSystem js;
    js.init();
    std::atomic<bool> afterThrowingRan{false};
    std::atomic<bool> laterCompleteRan{false};

    js.submit([]() { throw std::runtime_error("worker boom"); });
    js.submit(
        [&afterThrowingRan]() { afterThrowingRan.store(true); },
        JobPriority::Normal,
        [&laterCompleteRan]() { laterCompleteRan.store(true); });

    // Must return (not wedge) and both the later task + its callback run.
    js.waitIdle();
    for (int i = 0; i < 200 && !laterCompleteRan.load(); ++i) {
        js.pollMainThreadJobs();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    CHECK(afterThrowingRan.load());
    CHECK(laterCompleteRan.load());
    CHECK(js.pendingJobs() == 0);
    js.shutdown();
}

// (h) Exception isolation for onComplete on the REAL JobSystem: a throwing
// main-thread callback does not escape pollMainThreadJobs nor stop the rest of
// the drained batch.
TEST_CASE("Boundary[JobSystem] exception isolation: throwing onComplete is isolated during poll") {
    JobSystem js;
    js.init();
    std::atomic<bool> workerDone{false};
    std::atomic<bool> laterMainDone{false};

    // First job's work succeeds but its onComplete throws.
    js.submit(
        [&workerDone]() { workerDone.store(true); },
        JobPriority::Normal,
        []() { throw std::runtime_error("cb boom"); });
    // A second job whose onComplete succeeds must still be delivered + run.
    js.submit(
        []() { /* plain work */ },
        JobPriority::Normal,
        [&laterMainDone]() { laterMainDone.store(true); });

    for (int i = 0; i < 200 && !laterMainDone.load(); ++i) {
        js.waitIdle();
        js.pollMainThreadJobs();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    CHECK(workerDone.load());
    CHECK(laterMainDone.load());  // the batch kept draining past the throwing callback
    CHECK(js.pendingJobs() == 0);
    js.shutdown();
}

// =============================================================================
// Round-2 job boundary tests (round 81 follow-up)
// Covers: submit-before-init rejection, double shutdown, submit-after-shutdown,
// empty waitIdle, large concurrent fan-out completion, and non-std::exception
// isolation for both work() and onComplete() on the real JobSystem + Null mock.
// Semantics locked against src/job/JobSystem.cpp and mocks/NullJobSystem.h.
// =============================================================================

TEST_CASE("Boundary[JobSystem] submit before init is rejected (id 0), task never runs") {
    JobSystem js;  // NOT initialized
    bool ran = false;
    uint64_t id = js.submit([&]() { ran = true; });
    CHECK(id == 0);
    CHECK(ran == false);
    CHECK(js.isRunning() == false);
    CHECK(js.pendingJobs() == 0);
    js.shutdown();  // shutdown of an uninitialized system is a safe no-op
}

TEST_CASE("Boundary[Null] submit before init is rejected (id 0), task never runs") {
    NullJobSystem njs;  // NOT initialized
    bool ran = false;
    uint64_t id = njs.submit([&]() { ran = true; });
    CHECK(id == 0);
    CHECK(ran == false);
    CHECK(njs.isRunning() == false);
}

TEST_CASE("Boundary[JobSystem] double shutdown is idempotent and safe") {
    JobSystem js;
    js.init();
    REQUIRE(js.isRunning());
    CHECK(js.workerCount() >= 1);

    js.shutdown();
    CHECK(js.isRunning() == false);
    CHECK(js.workerCount() == 0);

    js.shutdown();  // second call must be a no-op, not crash
    CHECK(js.isRunning() == false);
    CHECK(js.workerCount() == 0);
}

TEST_CASE("Boundary[JobSystem] submit after shutdown is rejected (id 0)") {
    JobSystem js;
    js.init();
    js.shutdown();
    CHECK(js.isRunning() == false);

    bool ran = false;
    uint64_t id = js.submit([&]() { ran = true; });
    CHECK(id == 0);
    CHECK(ran == false);   // never scheduled, never ran
    CHECK(js.pendingJobs() == 0);
}

TEST_CASE("Boundary[JobSystem] waitIdle on an empty queue returns promptly") {
    JobSystem js;
    js.init();
    // Nothing submitted: waitIdle must not block, wedge, or misreport.
    js.waitIdle();
    CHECK(js.pendingJobs() == 0);
    CHECK(js.isRunning());

    // Also safe immediately after shutdown (workers already joined).
    js.shutdown();
    js.waitIdle();  // no-op after shutdown
    CHECK(js.pendingJobs() == 0);
}

TEST_CASE("Boundary[JobSystem] large concurrent fan-out completes exactly once per job") {
    JobSystem js;
    js.init();
    constexpr int kJobs = 64;
    std::atomic<int> workCount{0};
    std::atomic<int> completeCount{0};

    // Half the batch carries an onComplete; every job increments workCount.
    for (int i = 0; i < kJobs; ++i) {
        if (i % 2 == 0) {
            js.submit([&]() { workCount.fetch_add(1); },
                      JobPriority::Normal,
                      [&]() { completeCount.fetch_add(1); });
        } else {
            js.submit([&]() { workCount.fetch_add(1); });
        }
    }

    js.waitIdle();
    // Drain any trailing onComplete queued at the tail of the batch.
    for (int i = 0; i < 200 && completeCount.load() < kJobs / 2; ++i) {
        js.pollMainThreadJobs();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    CHECK(workCount.load() == kJobs);
    CHECK(completeCount.load() == kJobs / 2);
    CHECK(js.pendingJobs() == 0);
    js.shutdown();
}

TEST_CASE("Boundary[JobSystem] non-std::exception in work is isolated, queue keeps draining") {
    JobSystem js;
    js.init();

    // A work() that throws an int (not a std::exception) must be caught by the
    // catch(...) branch and swallowed so the worker survives and the queue drains.
    std::atomic<bool> afterRan{false};
    js.submit([]() { throw 42; });  // int throw
    js.submit([]() { struct C {}; throw C{}; });  // custom type throw
    js.submit([&]() { afterRan.store(true); });

    REQUIRE_NOTHROW(js.waitIdle());  // must not propagate the int/custom throw
    CHECK(afterRan.load());
    CHECK(js.pendingJobs() == 0);
    js.shutdown();
}

TEST_CASE("Boundary[Null] non-std::exception in work is isolated, submit returns normally") {
    NullJobSystem njs;
    njs.init();
    bool laterRan = false;
    uint64_t id = njs.submit([]() { throw 42; });  // int throw
    CHECK(id > 0);  // isolation -> submit still succeeded
    CHECK(njs.submit([&]() { laterRan = true; }) > 0);
    CHECK(laterRan);  // system still usable
}

TEST_CASE("Boundary[JobSystem] non-std::exception in onComplete is isolated during poll") {
    JobSystem js;
    js.init();
    std::atomic<bool> workerDone{false};
    std::atomic<bool> laterMainDone{false};

    // First callback throws an int; a later callback must still be delivered.
    js.submit(
        [&]() { workerDone.store(true); },
        JobPriority::Normal,
        []() { throw 7; });  // int throw in onComplete
    js.submit(
        []() { /* plain work */ },
        JobPriority::Normal,
        [&]() { laterMainDone.store(true); });

    for (int i = 0; i < 200 && !laterMainDone.load(); ++i) {
        js.waitIdle();
        js.pollMainThreadJobs();  // isolates the int-throwing callback
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    CHECK(workerDone.load());
    CHECK(laterMainDone.load());  // later callback still ran after the int throw
    CHECK(js.pendingJobs() == 0);
    js.shutdown();
}

TEST_CASE("Boundary[JobSystem] worker count never exceeds available hardware") {
    JobSystem js;
    js.init();
    const unsigned hw = std::thread::hardware_concurrency();
    // computeWorkerCount: hw==0 -> 4; hw<4 -> 1; else hw-1. In every branch the
    // worker count is positive and does NOT exceed the reported hardware
    // concurrency (the hw==0 -> 4 case still satisfies count <= max(hw,4)).
    CHECK(js.workerCount() >= 1);
    CHECK(static_cast<unsigned>(js.workerCount()) <= (hw == 0 ? 4u : hw));
    js.shutdown();
    CHECK(js.workerCount() == 0);
}

TEST_CASE("Boundary[Null] shutdown and re-init resets the job counter") {
    // NullJobSystem::shutdown() resets m_jobId to 1, so a fresh init cycle
    // issues ids from 1 again -- a clean, deterministic restart contract.
    NullJobSystem njs;
    njs.init();
    CHECK(njs.submit([]() {}) == 1);
    CHECK(njs.submit([]() {}) == 2);
    njs.shutdown();
    CHECK(njs.isRunning() == false);

    njs.init();
    CHECK(njs.submit([]() {}) == 1);  // counter reset after restart
    CHECK(njs.submit([]() {}) == 2);
    njs.shutdown();
}
