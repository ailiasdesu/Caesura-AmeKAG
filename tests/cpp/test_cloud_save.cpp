// test_cloud_save.cpp - HTTP cloud-save provider (C7) round trip against a
// local mock REST server: configure -> save slot -> push -> pull -> verify.
#include "doctest.h"
#include "TestPaths.h"
#include "storage/SaveManager.h"
#include "storage/HttpCloudSaveProvider.h"
#include "storage/CloudSaveProvider.h"
#include "storage/api/ISaveProvider.h"
#include "steam/api/ISteamBackend.h"
#include "di/BackendRegistry.h"
#include <httplib.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <map>
#include <nlohmann_json.hpp>
#include <thread>
#include <map>
#include <mutex>
#include <string>
#include <fstream>
#include <cstdio>

using namespace Caesura;

namespace {

// In-memory Steam Remote Storage stand-in. Kept in an anonymous namespace with
// its own name: test_storage.cpp defines a file-scope MockSteamBackend, and two
// different definitions of the same external-linkage class would be an ODR
// violation across translation units.
class CloudMockSteam final : public ISteamBackend {
public:
    std::map<std::string, std::string> files;

    bool init() override { return true; }
    void shutdown() override {}
    void runCallbacks() override {}
    bool isOverlayActive() const override { return false; }
    bool unlockAchievement(const char*) override { return true; }
    bool isAchievementUnlocked(const char*) const override { return false; }
    bool resetAchievement(const char*) override { return true; }
    bool resetAllAchievements() override { return true; }
    bool setStatInt(const char*, int32_t) override { return true; }
    int32_t getStatInt(const char*) const override { return 0; }
    bool setStatFloat(const char*, float) override { return true; }
    float getStatFloat(const char*) const override { return 0.0f; }
    bool storeStats() override { return true; }
    bool cloudWrite(const char* fileName, const void* data, int32_t size) override {
        if (!fileName || size < 0) return false;
        files[fileName] = std::string(static_cast<const char*>(data),
                                      static_cast<size_t>(size));
        return true;
    }
    int32_t cloudRead(const char* fileName, void* buffer, int32_t maxSize) override {
        const auto it = files.find(fileName ? fileName : "");
        if (it == files.end() || !buffer || maxSize <= 0) return 0;
        const int32_t n = std::min<int32_t>(maxSize,
                                            static_cast<int32_t>(it->second.size()));
        std::memcpy(buffer, it->second.data(), static_cast<size_t>(n));
        return n;
    }
    int32_t cloudFileSize(const char* fileName) const override {
        const auto it = files.find(fileName ? fileName : "");
        return it == files.end() ? 0 : static_cast<int32_t>(it->second.size());
    }
    bool cloudFileExists(const char* fileName) const override {
        return files.count(fileName ? fileName : "") > 0;
    }
    bool cloudDelete(const char* fileName) override {
        files.erase(fileName ? fileName : "");
        return true;
    }
    int32_t cloudQuotaTotal() const override { return 8 * 1024 * 1024; }
    int32_t cloudQuotaUsed() const override { return 0; }
    int32_t cloudFileCount() const override {
        return static_cast<int32_t>(files.size());
    }
    const char* cloudFileNameAt(int32_t index) const override {
        if (index < 0) return "";
        int32_t i = 0;
        for (const auto& entry : files) {
            if (i++ == index) return entry.first.c_str();
        }
        return "";
    }
    const char* name() const override { return "cloud-mock-steam"; }
};

}  // namespace

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

TEST_CASE("SaveManager::configureCloudSync steam endpoint requires a backend") {
    // No Steam backend is registered in the test process, so the steam endpoint
    // must FAIL CLOSED and keep the existing provider. Installing a
    // CloudSaveProvider over a null backend would make load()/listSaves()
    // report every existing save as gone (readFile returns "" for all of them).
    TestPaths::ScopedTempDir dir("cloud_steam_nobackend");
    SaveManager mgr;
    mgr.init(dir.string());
    REQUIRE(mgr.configureCloudSync(""));  // local provider installed
    ISaveProvider* before = mgr.getSaveProvider();
    REQUIRE(before != nullptr);

    // A save made locally must still be visible after the refused switch.
    REQUIRE(mgr.save(3, nlohmann::json{{"hp", 7}}, "chapter_1", 11));
    REQUIRE(mgr.slotExists(3));

    for (const char* endpoint : {"steam", "steam://", "steamcloud"}) {
        CAPTURE(endpoint);
        CHECK_FALSE(mgr.configureCloudSync(endpoint));
        CHECK(mgr.getSaveProvider() == before);  // provider untouched
        CHECK(mgr.slotExists(3));                // save still reachable
    }

    // Local-only provider has no cloud end: push/pull refuse and say so
    // (t5 finding -- these used to be indistinguishable from a failed transfer).
    CHECK_FALSE(mgr.pushSlotToCloud(3));
    CHECK_FALSE(mgr.pullSlotFromCloud(3));
    CHECK(mgr.slotExists(3));  // a refused sync never touches the save
}

// End-to-end save -> load round trip under the steam endpoint. This is the
// case t14 asks about: once CloudSaveProvider is installed, Steam Remote
// Storage IS the store, so the round trip must work without any local file.
TEST_CASE("SaveManager: steam endpoint save/load round trip goes through the cloud") {
    CloudMockSteam steam;
    BackendRegistry::instance().setSteamBackend(&steam);
    struct Restore {
        ~Restore() { BackendRegistry::instance().setSteamBackend(nullptr); }
    } restore;

    TestPaths::ScopedTempDir dir("cloud_steam_roundtrip");
    SaveManager mgr;
    mgr.init(dir.string());
    REQUIRE(mgr.configureCloudSync("steam"));
    REQUIRE(mgr.getSaveProvider() != nullptr);
    REQUIRE(mgr.getSaveProvider()->supportsCloudSync());

    const nlohmann::json data = {{"scene", "chapter_7"}, {"affinity", 88}};
    REQUIRE(mgr.save(5, data, "chapter_7", 314));

    // The bytes landed in Steam Remote Storage under the FLAT key, not on disk.
    CHECK(steam.files.count("save_5.json") == 1);
    CHECK_FALSE(std::filesystem::exists(dir.path() / "save_5.json"));

    // Round trip: load() reads back through the same provider.
    SaveMeta meta;
    const nlohmann::json loaded = mgr.load(5, &meta);
    REQUIRE(loaded.is_object());
    CHECK(loaded.value("affinity", 0) == 88);
    CHECK(meta.sceneName == "chapter_7");
    CHECK(meta.tokenIndex == 314);
    CHECK(mgr.slotExists(5));

    // listSaves() enumerates the cloud store too.
    bool listed = false;
    for (const auto& m : mgr.listSaves()) {
        if (m.slot == 5) listed = true;
    }
    CHECK(listed);

    // delete removes the cloud object.
    CHECK(mgr.deleteSlot(5));
    CHECK(steam.files.count("save_5.json") == 0);
    CHECK_FALSE(mgr.slotExists(5));
}

// Who wins when local and cloud disagree? Nobody implicitly: each direction is
// an explicit call, and the side named by the call wins for that call only.
TEST_CASE("CloudSaveProvider: push and pull are explicit one-way transfers") {
    CloudMockSteam steam;
    CloudSaveProvider provider(&steam);

    TestPaths::ScopedTempDir dir("cloud_conflict");
    const std::string localPath = (dir.path() / "save_9.json").string();

    // Local says "local-newer", cloud says "cloud-older".
    {
        std::ofstream f(localPath, std::ios::binary | std::ios::trunc);
        f << "local-newer";
    }
    steam.files["save_9.json"] = "cloud-older";

    // push: local wins, cloud replaced. The directory component is stripped, so
    // the cloud key stays flat.
    REQUIRE(provider.pushToCloud(localPath));
    CHECK(steam.files["save_9.json"] == "local-newer");

    // pull: cloud wins, local file replaced.
    steam.files["save_9.json"] = "cloud-authoritative";
    REQUIRE(provider.pullFromCloud(localPath));
    {
        std::ifstream f(localPath, std::ios::binary);
        std::string got((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
        CHECK(got == "cloud-authoritative");
    }

    // A missing cloud object must NOT wipe the local save: pull aborts before
    // opening the local file for writing.
    steam.files.erase("save_9.json");
    CHECK_FALSE(provider.pullFromCloud(localPath));
    {
        std::ifstream f(localPath, std::ios::binary);
        std::string got((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
        CHECK(got == "cloud-authoritative");  // untouched by the failed pull
    }

    // An empty cloud object is treated the same way (no silent truncation).
    steam.files["save_9.json"] = "";
    CHECK_FALSE(provider.pullFromCloud(localPath));
    {
        std::ifstream f(localPath, std::ios::binary);
        std::string got((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
        CHECK(got == "cloud-authoritative");
    }

    // push with no local file must not fabricate a cloud write from the cloud's
    // own copy (the earlier revision looped cloud -> cloud and reported true).
    steam.files["save_absent.json"] = "cloud-only";
    CHECK_FALSE(provider.pushToCloud((dir.path() / "save_absent.json").string()));
    CHECK(steam.files["save_absent.json"] == "cloud-only");  // unchanged
}

TEST_CASE("CloudSaveProvider: null backend refuses both transfer directions") {
    CloudSaveProvider provider(nullptr);
    TestPaths::ScopedTempDir dir("cloud_null_backend");
    const std::string localPath = (dir.path() / "save_0.json").string();
    {
        std::ofstream f(localPath, std::ios::binary | std::ios::trunc);
        f << "local-data";
    }
    CHECK_FALSE(provider.pushToCloud(localPath));
    CHECK_FALSE(provider.pullFromCloud(localPath));
    // The local file survives a refused pull.
    std::ifstream f(localPath, std::ios::binary);
    std::string got((std::istreambuf_iterator<char>(f)),
                    std::istreambuf_iterator<char>());
    CHECK(got == "local-data");
}

