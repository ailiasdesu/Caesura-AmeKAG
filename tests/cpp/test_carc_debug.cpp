#include "doctest.h"
#include "archive/CARCWriter.h"
#include "archive/CARCReader.h"
#include <thread>
#include <atomic>
#include <cstdio>
#include <filesystem>

using namespace Caesura::carc;

TEST_CASE("debug: sequential open in thread") {
    const char* path = "test_seq.carc";
    std::filesystem::remove(path);
    {
        CARCWriter w;
        w.create(path);
        w.addFile("f.txt", (const uint8_t*)"hello", 5);
        w.finalize();
    }

    std::atomic<bool> ok{false};
    std::thread t([&]() {
        CARCReader r;
        bool opened = r.open(path);
        printf("[thread] open=%d hasFile=%d numFiles=%zu\n", opened, r.hasFile("f.txt"), r.numFiles());
        if (opened && r.hasFile("f.txt")) {
            auto data = r.readFile("f.txt");
            printf("[thread] data.size=%zu\n", data.size());
            if (data.size() == 5) ok.store(true);
        }
    });
    t.join();
    CHECK(ok.load());
    std::filesystem::remove(path);
}
