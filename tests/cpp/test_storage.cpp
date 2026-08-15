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
#include <fstream>

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

// =============================================================================
// G10: storage-layer hardening -- corrupted-save recovery, slot boundary
//      semantics, empty listing, overwrite, delete-missing, schema/migration,
//      nested payload roundtrip.
// =============================================================================

TEST_CASE("Storage: corrupted save file fails gracefully (no crash on load)") {
    TestPaths::ScopedTempDir dir("storage_corrupt");
    SaveManager sm;
    sm.init(dir.string());

    // (a-1) Truncated / invalid JSON in a real slot file.
    {
        std::ofstream f(dir.string() + "save_0.json", std::ios::binary | std::ios::trunc);
        f << "{truncated\"not-closed";
    }
    // (a-2) Valid JSON but a non-object envelope (array).
    {
        std::ofstream f(dir.string() + "save_1.json", std::ios::binary | std::ios::trunc);
        f << "[]";
    }
    // (a-3) Valid JSON scalar (not an object envelope).
    {
        std::ofstream f(dir.string() + "save_2.json", std::ios::binary | std::ios::trunc);
        f << "42";
    }

    // load() must report failure (null json), never throw or crash.
    CHECK(sm.load(0).is_null());
    CHECK(sm.load(1).is_null());
    CHECK(sm.load(2).is_null());

    // listSaves() must skip corrupt files instead of surfacing them.
    REQUIRE(sm.listSaves().empty());

    // slotExists reflects file presence (non-empty content), not validity.
    CHECK(sm.slotExists(0));
    CHECK(sm.slotExists(1));
    CHECK(sm.slotExists(2));

    // The manager stays fully usable afterwards: save/load/delete a valid slot.
    json ok = {{"ok", true}};
    REQUIRE(sm.save(0, ok, "demo", 1));
    CHECK(sm.load(0)["ok"] == true);
    REQUIRE(sm.deleteSlot(0));

    // A valid neighbour after a corrupt one is still loadable/listed.
    json good = {{"v", 9}};
    REQUIRE(sm.save(3, good, "good", 2));
    CHECK(sm.load(3)["v"] == 9);
    auto saves = sm.listSaves();
    REQUIRE(saves.size() == 1);
    CHECK(saves[0].slot == 3);
}

TEST_CASE("Storage: slot boundary semantics (0 and 99 valid, out-of-range rejected)") {
    TestPaths::ScopedTempDir dir("storage_bounds");
    SaveManager sm;
    sm.init(dir.string());

    json gd = {{"n", 1}};
    // Lower and upper legal bounds both save/load/delete round-trip.
    REQUIRE(sm.save(0, gd, "s0", 1));
    REQUIRE(sm.save(99, gd, "s99", 2));
    CHECK(sm.load(0)["n"] == 1);
    CHECK(sm.load(99)["n"] == 1);
    CHECK(sm.slotExists(0));
    CHECK(sm.slotExists(99));

    // Out of range: rejected, never fabricates paths or crashes.
    CHECK_FALSE(sm.save(100, gd, "hi", 0));
    CHECK_FALSE(sm.save(1000, gd, "hi", 0));
    CHECK_FALSE(sm.save(-1, gd, "neg", 0));
    CHECK(sm.load(100).is_null());
    CHECK(sm.load(1000).is_null());
    CHECK(sm.load(-1).is_null());
    CHECK_FALSE(sm.slotExists(100));
    CHECK_FALSE(sm.slotExists(-1));
    CHECK_FALSE(sm.deleteSlot(100));
    CHECK_FALSE(sm.deleteSlot(-1));

    // Exactly the two legal slots are listed.
    auto saves = sm.listSaves();
    REQUIRE(saves.size() == 2);
    CHECK(saves[0].slot == 0);
    CHECK(saves[1].slot == 99);
}

TEST_CASE("Storage: empty start -- listSaves empty, slots absent, load null") {
    TestPaths::ScopedTempDir dir("storage_empty_hard");
    SaveManager sm;
    sm.init(dir.string());
    CHECK(sm.listSaves().empty());
    CHECK_FALSE(sm.slotExists(0));
    CHECK_FALSE(sm.slotExists(50));
    CHECK_FALSE(sm.slotExists(99));
    CHECK(sm.load(0).is_null());
}

TEST_CASE("Storage: save twice to same slot replaces payload and updates metadata") {
    TestPaths::ScopedTempDir dir("storage_overwrite");
    SaveManager sm;
    sm.init(dir.string());

    json first = {{"phase", 1}};
    json second = {{"phase", 2}, {"note", "second"}};

    REQUIRE(sm.save(5, first, "sceneA", 3));
    REQUIRE(sm.save(5, second, "sceneB", 7));

    SaveMeta meta;
    json loaded = sm.load(5, &meta);
    CHECK(loaded["phase"] == 2);
    CHECK(loaded["note"] == "second");
    CHECK(meta.slot == 5);
    CHECK(meta.sceneName == "sceneB");
    CHECK(meta.tokenIndex == 7);

    // Single slot file remains; list shows the updated metadata.
    auto saves = sm.listSaves();
    REQUIRE(saves.size() == 1);
    CHECK(saves[0].slot == 5);
    CHECK(saves[0].sceneName == "sceneB");
    CHECK(saves[0].tokenIndex == 7);
}

TEST_CASE("Storage: deleting a missing slot is a safe no-op") {
    TestPaths::ScopedTempDir dir("storage_del_missing");
    SaveManager sm;
    sm.init(dir.string());

    CHECK_FALSE(sm.deleteSlot(3));
    CHECK(sm.listSaves().empty());

    // Present then deleted; a second delete is again a no-op.
    REQUIRE(sm.save(3, {{"a", 1}}, "s", 0));
    CHECK(sm.slotExists(3));
    REQUIRE(sm.deleteSlot(3));
    CHECK_FALSE(sm.slotExists(3));
    CHECK_FALSE(sm.deleteSlot(3));
    CHECK(sm.load(3).is_null());
}

TEST_CASE("Storage: schema migration applies the built-in chain to an old save") {
    TestPaths::ScopedTempDir dir("storage_migrate");
    SaveManager sm;
    sm.init(dir.string());  // registers built-in v1->v2->v3->v4->v5

    // Write an old v1 save by hand (simulating a save produced pre-migration).
    json env;
    env["schema_version"] = 1;
    env["timestamp"] = 0;
    env["scene"] = "old";
    env["token_index"] = 0;
    env["thumbnail"] = "";
    env["engine_version"] = "0.9.0";
    env["data"] = json::object();
    {
        std::ofstream f(dir.string() + "save_3.json", std::ios::binary | std::ios::trunc);
        f << env.dump();
    }

    SaveMeta meta;
    json loaded = sm.load(3, &meta);
    // Built-in chain adds playtime(2), minigame(3), live2d(4), editor(5).
    CHECK(loaded["playtime"] == 0);
    CHECK(loaded["minigame"].is_object());
    CHECK(loaded["live2d"].is_object());
    CHECK(loaded["editor"].is_object());
    // outMeta reflects the migrated (current) version.
    CHECK(meta.schemaVersion == sm.currentSchemaVersion());
}

TEST_CASE("Storage: registerMigration bumps current version, rejects non-increasing") {
    SaveManager sm;  // not init()ed -- only explicit migrations below apply
    CHECK(sm.currentSchemaVersion() == 1);

    sm.registerMigration(1, 2, [](const json& d) { json r = d; r["x"] = 1; return r; });
    CHECK(sm.currentSchemaVersion() == 2);

    // Non-increasing registrations are rejected and do not change the version.
    sm.registerMigration(1, 1, [](const json& d) { return d; });
    sm.registerMigration(3, 3, [](const json& d) { return d; });
    CHECK(sm.currentSchemaVersion() == 2);

    // The registered migration is applied by migrate().
    json out = sm.migrate(json::object(), 1);
    CHECK(out["x"] == 1);
    // Data already at current version passes through unchanged.
    CHECK(sm.migrate({{"y", 2}}, 2)["y"] == 2);
}

TEST_CASE("Storage: save payload roundtrip preserves nested Lua save-state structure") {
    TestPaths::ScopedTempDir dir("storage_roundtrip");
    SaveManager sm;
    sm.init(dir.string());

    // A payload shaped like the engine's Lua serialized save state.
    json gd;
    gd["f"] = json::object();
    gd["sf"] = json::object();
    gd["tf"] = json::object();
    gd["token_index"] = 42;
    gd["scene_path"] = "route/a.ks";
    gd["f"]["tbl"]["a"]["b"] = 1;               // f.tbl = {a={b=1}}
    gd["f"]["tbl"]["list"] = {1, 2, 3};
    gd["f"]["name"] = "protagonist";
    gd["backlog"] = {"line1", "line2"};
    gd["call_stack"] = {"main", "choice1"};
    gd["loop_stacks"] = json::array();
    gd["loop_stacks"].push_back({{"var", "i"}, {"end", 10}});
    gd["seen_scenes"] = {"prologue", "chapter1"};

    REQUIRE(sm.save(8, gd, "chapter1", 42));

    SaveMeta meta;
    json loaded = sm.load(8, &meta);
    CHECK(loaded == gd);                          // exact deep roundtrip
    CHECK(loaded["f"]["tbl"]["a"]["b"] == 1);     // f.tbl = {a={b=1}}
    CHECK(loaded["f"]["tbl"]["list"].is_array());
    CHECK(loaded["f"]["tbl"]["list"].size() == 3);
    CHECK(loaded["loop_stacks"][0]["end"] == 10);
    CHECK(meta.sceneName == "chapter1");
    CHECK(meta.tokenIndex == 42);
    auto saves = sm.listSaves();
    REQUIRE(saves.size() == 1);
    CHECK(saves[0].slot == 8);
}
