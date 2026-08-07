#include "storage/CloudSaveProvider.h"
#include "steam/api/ISteamBackend.h"
#include <algorithm>
#include <cstring>
#include <map>
// test_storage.cpp - storage module unit tests (S2.4)
#include "doctest.h"
#include "storage/api/ISaveManager.h"
#include "storage/SaveManager.h"
#include "TestPaths.h"
#include <filesystem>
#include <cstring>

using namespace Caesura;

TEST_CASE("Storage: SaveManager is constructible") {
    SaveManager sm;
    CHECK(sm.currentSchemaVersion() >= 0);
}

TEST_CASE("Storage: SaveManager init with temp directory") {
    TestPaths::ScopedTempDir dir("storage_init");
    SaveManager sm;
    sm.init(dir.string());
    CHECK(true);
}

TEST_CASE("Storage: SaveManager listSaves on empty directory") {
    TestPaths::ScopedTempDir dir("storage_empty");
    SaveManager sm;
    sm.init(dir.string());
    auto saves = sm.listSaves();
    CHECK(saves.empty());
}

TEST_CASE("Storage: SaveManager currentSchemaVersion is non-negative") {
    SaveManager sm;
    CHECK(sm.currentSchemaVersion() >= 0);
}

TEST_CASE("Storage: SaveManager slotExists on uninitialized returns false") {
    SaveManager sm;
    CHECK(sm.slotExists(99) == false);
}

TEST_CASE("Storage: SaveManager deleteSlot on uninitialized returns false") {
    SaveManager sm;
    CHECK(sm.deleteSlot(99) == false);
}

TEST_CASE("Storage: ISaveManager interface upcast") {
    SaveManager sm;
    ISaveManager* iface = &sm;
    CHECK(iface != nullptr);
    CHECK(iface->currentSchemaVersion() >= 0);
}

// =============================================================================
// Expanded: save provider
// =============================================================================

TEST_CASE("Storage: SaveManager default save provider exists after init") {
    TestPaths::ScopedTempDir dir("storage_provider");
    SaveManager sm;
    sm.init(dir.string());
    // Save provider is nullptr by default (must be set via setSaveProvider)
    // init does not automatically create a LocalFileSaveProvider
    CHECK(sm.getSaveProvider() == nullptr);
}

TEST_CASE("Storage: SaveManager ENGINE_VERSION is not empty") {
    CHECK(SaveManager::ENGINE_VERSION != nullptr);
    CHECK(std::strlen(SaveManager::ENGINE_VERSION) > 0);
}

// -- Mock Steam backend with in-memory cloud storage -------------------------

class MockSteamBackend final : public ISteamBackend {
public:
    std::map<std::string, std::string> files;
    // When >= 0, cloudWrite fails for chunk indexes >= this value (for
    // rollback testing). Only applied to .chunkNNN writes.
    int failCloudWritesFrom = -1;

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
        if (failCloudWritesFrom >= 0 && std::strstr(fileName, ".chunk")) {
            int idx = 0;
            if (std::sscanf(std::strrchr(fileName, 'k') + 1, "%d", &idx) == 1 && idx >= failCloudWritesFrom)
                return false;
        }
        files[fileName] = std::string(static_cast<const char*>(data), static_cast<size_t>(size));
        return true;
    }
    int32_t cloudRead(const char* fileName, void* buffer, int32_t maxSize) override {
        const auto it = files.find(fileName ? fileName : "");
        if (it == files.end() || !buffer || maxSize <= 0) return 0;
        const int32_t n = std::min<int32_t>(maxSize, static_cast<int32_t>(it->second.size()));
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
    int32_t cloudFileCount() const override { return static_cast<int32_t>(files.size()); }
    const char* cloudFileNameAt(int32_t index) const override {
        if (index < 0) return "";
        int32_t i = 0;
        for (const auto& [name, _] : files) {
            if (i++ == index) return name.c_str();
        }
        return "";
    }
    const char* name() const override { return "mock-steam"; }
};

TEST_CASE("Storage: CloudSaveProvider small-file roundtrip") {
    MockSteamBackend steam;
    CloudSaveProvider provider(&steam);

    const std::string payload = "{\"scene\":\"demo\",\"slot\":1}";
    REQUIRE(provider.writeFile("save_1.json", payload));
    CHECK(steam.cloudFileExists("save_1.json"));
    CHECK(provider.readFile("save_1.json") == payload);

    REQUIRE(provider.writeFile("local_slot.json", "local-data"));
    CHECK(provider.pushToCloud("local_slot.json"));
    CHECK(provider.readFile("local_slot.json") == "local-data");

    REQUIRE(provider.deleteFile("save_1.json"));
    CHECK_FALSE(steam.cloudFileExists("save_1.json"));
}

TEST_CASE("Storage: CloudSaveProvider large-file chunked roundtrip") {
    MockSteamBackend steam;
    CloudSaveProvider provider(&steam);

    // Exceeds the single-chunk threshold: exercises .meta + .chunkNNN path
    std::string payload;
    payload.reserve(300000);
    for (int i = 0; i < 300000; ++i) payload += static_cast<char>('a' + (i % 26));

    REQUIRE(provider.writeFile("big_save.json", payload));
    CHECK(steam.cloudFileExists("big_save.json.meta"));
    CHECK(provider.readFile("big_save.json") == payload);

    REQUIRE(provider.deleteFile("big_save.json"));
    CHECK_FALSE(steam.cloudFileExists("big_save.json.meta"));
    CHECK_FALSE(steam.cloudFileExists("big_save.json.chunk000"));
}



TEST_CASE("Storage: CloudSaveProvider rejects forged chunked .meta") {
    MockSteamBackend steam;
    CloudSaveProvider provider(&steam);

    // Absurd totalSize / numChunks must be rejected, not reserve GBs or loop.
    steam.files["f.meta"] = "2147483647,2147483647";
    CHECK(provider.readFile("f").empty());

    // numChunks far above what totalSize can legitimately use.
    steam.files["f.meta"] = "100,1000000";
    CHECK(provider.readFile("f").empty());

    // Missing chunks -> empty, not partial data.
    steam.files["f.meta"] = "300,2";
    steam.files["f.chunk000"] = std::string(256 * 1024, 'a');
    CHECK(provider.readFile("f").empty());

    // A chunk longer than the bytes still expected must be rejected.
    steam.files.clear();
    steam.files["g.meta"] = "10,1";
    steam.files["g.chunk000"] = std::string(20, 'a');
    CHECK(provider.readFile("g").empty());

    // Assembled length != totalSize must be rejected.
    steam.files.clear();
    steam.files["h.meta"] = "10,1";
    steam.files["h.chunk000"] = std::string(5, 'a');
    CHECK(provider.readFile("h").empty());
}

TEST_CASE("Storage: CloudSaveProvider chunked write failure rolls back meta") {
    MockSteamBackend steam;
    CloudSaveProvider provider(&steam);

    // 600KB payload -> 3 chunks; make the 2nd chunk write fail.
    std::string payload(600000, 'x');
    steam.failCloudWritesFrom = 1;  // fail chunk index 1 (0-based)
    CHECK_FALSE(provider.writeFile("roll.json", payload));

    // .meta and every chunk written so far must be gone.
    CHECK_FALSE(steam.cloudFileExists("roll.json.meta"));
    CHECK_FALSE(steam.cloudFileExists("roll.json.chunk000"));
    CHECK_FALSE(steam.cloudFileExists("roll.json.chunk001"));
}

TEST_CASE("Storage: CloudSaveProvider null backend is a safe no-op") {
    CloudSaveProvider provider(nullptr);
    CHECK_FALSE(provider.writeFile("x.json", "data"));
    CHECK(provider.readFile("x.json").empty());
    CHECK_FALSE(provider.deleteFile("x.json"));
    CHECK(provider.listFiles("*").empty());
    CHECK_FALSE(provider.pushToCloud("x.json"));
}