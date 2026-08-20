// test_cloud_save.cpp - HTTP cloud-save provider (C7) round trip against a
// local mock REST server: configure -> save slot -> push -> pull -> verify.
#include "doctest.h"
#include "storage/SaveManager.h"
#include "storage/HttpCloudSaveProvider.h"
#include <httplib.h>
#include <nlohmann_json.hpp>
#include <thread>
#include <map>
#include <mutex>
#include <string>
#include <fstream>
#include <cstdio>

using namespace Caesura;

TEST_CASE("HttpCloudSaveProvider: push/pull round trip via mock server") {
    // Mock REST server: in-memory file store.
    httplib::Server srv;
    std::map<std::string, std::string> store;
    std::mutex storeMutex;
    srv.Put(R"(/saves/(.*))", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(storeMutex);
        store[req.matches[1]] = req.body;
        res.set_content("{}", "application/json");
    });
    srv.Get(R"(/saves/(.*))", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(storeMutex);
        auto it = store.find(req.matches[1]);
        if (it == store.end()) { res.status = 404; return; }
        res.set_content(it->second, "application/octet-stream");
    });
    srv.Delete(R"(/saves/(.*))", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(storeMutex);
        store.erase(req.matches[1]);
        res.set_content("{}", "application/json");
    });
    int port = 0;
    for (int p = 18961; p <= 18970 && port == 0; ++p) {
        if (srv.bind_to_port("127.0.0.1", p)) port = p;
    }
    REQUIRE(port != 0);
    std::thread t([&]() { srv.listen_after_bind(); });

    const std::string endpoint = "http://127.0.0.1:" + std::to_string(port) + "/saves";
    SaveManager mgr;
    mgr.init("cloud_test_saves");
    REQUIRE(mgr.configureCloudSync(endpoint));

    // Save slot 2 locally, push, wipe local, pull, verify.
    nlohmann::json data = {{"scene", "chapter_3"}, {"hp", 42}};
    REQUIRE(mgr.save(2, data, "chapter_3", 120));
    REQUIRE(mgr.pushSlotToCloud(2));
    {
        std::lock_guard<std::mutex> lock(storeMutex);
        REQUIRE(store.find("save_2.json") != store.end());
        CHECK(store["save_2.json"].find("chapter_3") != std::string::npos);
    }
    // Simulate another machine: fresh manager pulls the slot.
    SaveManager other;
    other.init("cloud_test_saves");
    REQUIRE(other.configureCloudSync(endpoint));
    REQUIRE(other.pullSlotFromCloud(2));
    CHECK(other.slotExists(2));
    auto meta = other.listSaves();
    bool found = false;
    for (const auto& m : meta) {
        if (m.slot == 2) found = true;
    }
    CHECK(found);

    // Delete on the server; offline degrade: unreachable endpoint -> false.
    mgr.deleteSlot(2);
    mgr.pushSlotToCloud(2);  // re-push (slot file gone -> false is fine)
    REQUIRE(mgr.configureCloudSync(""));  // back to local-only
    CHECK_FALSE(mgr.pushSlotToCloud(2));  // local provider: no cloud sync

    srv.stop();
    t.join();
    // Cleanup test dirs.
    std::remove("cloud_test_saves/slot_2.json");
    std::remove("cloud_test_saves/save_2.meta");
    std::remove("cloud_test_saves");
    std::remove("cloud_test_saves/index.json");
}

TEST_CASE("HttpCloudSaveProvider: offline degrade never throws") {
    // A port nothing listens on: push/pull return false, no exception.
    // Use a port in the freed range; nothing listens there.
    const int port = 18999;

    HttpCloudSaveProvider provider(
        "http://127.0.0.1:" + std::to_string(port) + "/saves", 500);
    // No local file -> push false without touching the network.
    CHECK_FALSE(provider.pushToCloud("saves/nope.json"));
    CHECK_FALSE(provider.pullFromCloud("saves/nope.json"));
}
TEST_CASE("HttpCloudSaveProvider: oversized cloud payload rejected (ST-2)") {
    // Mock server returning a body larger than the 10 MiB cap: pull must
    // reject it instead of writing a multi-GB local file.
    httplib::Server srv;
    srv.Get(R"(/saves/(.*))", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(std::string(11u * 1024u * 1024u, 'x'), "application/octet-stream");
    });
    int port = 0;
    for (int p = 18941; p <= 18950 && port == 0; ++p) {
        if (srv.bind_to_port("127.0.0.1", p)) port = p;
    }
    REQUIRE(port != 0);
    std::thread t([&]() { srv.listen_after_bind(); });

    // Direct provider: pull must return false and leave no local file.
    {
        HttpCloudSaveProvider provider(
            "http://127.0.0.1:" + std::to_string(port) + "/saves", 8000);
        CHECK_FALSE(provider.pullFromCloud("save_0.json"));
        // No local artifact written by the pull itself.
        std::ifstream f("save_0.json");
        CHECK_FALSE(f.good());
    }
    srv.stop();
    t.join();
    std::remove("save_0.json");
}

TEST_CASE("HttpCloudSaveProvider: https endpoint fails closed without SSL (ST-2)") {
    // Without CPPHTTPLIB_OPENSSL_SUPPORT the client must reject an https
    // endpoint (return false) rather than silently downgrade to plaintext.
    HttpCloudSaveProvider provider("https://example.com/saves", 500);
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    // With SSL compiled we cannot reach example.com from CI; just ensure no crash.
    (void)provider;
    CHECK(true);
#else
    CHECK_FALSE(provider.pushToCloud("saves/nope.json"));
    // pull returns false: no local file, no exception, TLS not downgraded.
    CHECK_FALSE(provider.pullFromCloud("saves/nope.json"));
#endif
}

TEST_CASE("HttpCloudSaveProvider: bearer token sent as Authorization header (ST-2)") {
    httplib::Server srv;
    std::string gotAuth;
    std::mutex authMutex;
    srv.Get(R"(/saves/(.*))", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(authMutex);
        gotAuth = req.get_header_value("Authorization");
        res.set_content("token-ok", "application/octet-stream");
    });
    int port = 0;
    for (int p = 18931; p <= 18940 && port == 0; ++p) {
        if (srv.bind_to_port("127.0.0.1", p)) port = p;
    }
    REQUIRE(port != 0);
    std::thread t([&]() { srv.listen_after_bind(); });

    {
        // Write a local file so pull has a target path to write into.
        HttpCloudSaveProvider provider(
            "http://127.0.0.1:" + std::to_string(port) + "/saves", 8000, "sekret");
        // Write a tiny local file first so push has content to send.
        // (pull reads the body regardless; the write target is the local file.)
        CHECK(provider.writeFile("st_ok.json", "seed"));
        const bool ok = provider.pullFromCloud("st_ok.json");
        CHECK(ok);
        std::lock_guard<std::mutex> lock(authMutex);
        CHECK(gotAuth == "Bearer sekret");
    }
    srv.stop();
    t.join();
    std::remove("st_ok.json");
}

