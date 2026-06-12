// test_null_jobsystem.cpp - NullJobSystem unit tests (IU-2)
#include "doctest.h"
#include "mocks/NullJobSystem.h"

using namespace Caesura;

TEST_CASE("NullJobSystem: init sets running") {
    NullJobSystem njs;
    CHECK(njs.isRunning() == false);
    njs.init();
    CHECK(njs.isRunning() == true);
}

TEST_CASE("NullJobSystem: submit executes work synchronously") {
    NullJobSystem njs;
    njs.init();
    bool workRan = false;
    uint64_t id = njs.submit([&]() { workRan = true; });
    CHECK(id > 0);
    CHECK(workRan == true);
}

TEST_CASE("NullJobSystem: submit executes onComplete synchronously") {
    NullJobSystem njs;
    njs.init();
    bool callbackRan = false;
    njs.submit([]() {}, JobPriority::Normal, [&]() { callbackRan = true; });
    CHECK(callbackRan == true);
}

TEST_CASE("NullJobSystem: shutdown stops running") {
    NullJobSystem njs;
    njs.init();
    CHECK(njs.isRunning() == true);
    njs.shutdown();
    CHECK(njs.isRunning() == false);
}

TEST_CASE("NullJobSystem: submit after shutdown returns 0") {
    NullJobSystem njs;
    njs.init();
    njs.shutdown();
    bool workRan = false;
    uint64_t id = njs.submit([&]() { workRan = false; });
    CHECK(id == 0);
    CHECK(workRan == false);
}

TEST_CASE("NullJobSystem: pendingJobs always 0") {
    NullJobSystem njs;
    njs.init();
    CHECK(njs.pendingJobs() == 0);
    njs.submit([]() {});
    CHECK(njs.pendingJobs() == 0);
    njs.shutdown();
    CHECK(njs.pendingJobs() == 0);
}

TEST_CASE("NullJobSystem: workerCount is 0") {
    NullJobSystem njs;
    CHECK(njs.workerCount() == 0);
    njs.init();
    CHECK(njs.workerCount() == 0);
}

TEST_CASE("NullJobSystem: multiple submits get unique IDs") {
    NullJobSystem njs;
    njs.init();
    uint64_t id1 = njs.submit([]() {});
    uint64_t id2 = njs.submit([]() {});
    uint64_t id3 = njs.submit([]() {});
    CHECK(id1 != id2);
    CHECK(id2 != id3);
    CHECK(id1 != id3);
}
