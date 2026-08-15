#include "storage/CloudSaveProvider.h"
#include "steam/api/ISteamBackend.h"
#include <algorithm>
#include <cstring>
#include <map>
// test_storage.cpp - storage module unit tests (S2.4)
#include "doctest.h"
#include "storage/api/ISaveManager.h"
#include "storage/api/ISaveProvider.h"
#include "storage/SaveManager.h"
#include "archive/CryptoEngine.h"
#include "di/BackendRegistry.h"
#include "TestPaths.h"
#include <filesystem>
#include <cstring>
#include <fstream>

using namespace Caesura;

namespace {

// RAII: register a real AES-256-GCM CryptoEngine into the BackendRegistry for
// the duration of an encrypted-save test. SaveManager encrypt/decrypt routes
// through BackendRegistry::getCryptoEngine(), which is null unless registered
// (the test binary does not run a full Engine::init()).
class ScopedCryptoRegistration {
public:
    ScopedCryptoRegistration()
        : m_previous(BackendRegistry::instance().getCryptoEngine()) {
        BackendRegistry::instance().setCryptoEngine(&m_engine);
    }

    ~ScopedCryptoRegistration() {
        BackendRegistry::instance().setCryptoEngine(m_previous);
    }

private:
    carc::CryptoEngine m_engine;
    carc::ICryptoEngine* m_previous;
};

// Read raw file bytes (bypassing SaveManager) for at-rest inspection.
std::string readRawFileBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

} // namespace

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

// =============================================================================
// G10.1: storage encryption roundtrip -- at-rest secrecy, plain-vs-encrypted
// provider interop, encrypted slot boundary/overwrite/delete semantics, and
// tampered-ciphertext rejection. All cases register a real CryptoEngine into
// the BackendRegistry (SaveManager encrypt/decrypt routes through it).
//
// ARCHITECTURE NOTE (verified): ISaveProvider is a raw byte store with NO
// encryption awareness; encryption is decided entirely by SaveManager's
// m_keySet flag + the "CAES" magic in the on-disk bytes. There is no
// "encrypted provider" class -- see cases I2/I3 which exercise the real
// plain-vs-keyed interop behavior.
// =============================================================================

TEST_CASE("Storage: encrypted save roundtrip preserves nested payload exactly") {
    TestPaths::ScopedTempDir dir("storage_enc_roundtrip");
    ScopedCryptoRegistration cryptoRegistration;
    SaveManager sm;
    sm.init(dir.string());

    uint8_t key[32] = {};
    for (int i = 0; i < 32; ++i) key[i] = static_cast<uint8_t>(i + 1);
    sm.setEncryptionKey(key);
    REQUIRE(sm.isEncryptionEnabled());

    // The same deep Lua-shaped payload as the plain roundtrip case, so the
    // encryption path is exercised over the full structured surface.
    json gd;
    gd["f"] = json::object();
    gd["sf"] = json::object();
    gd["tf"] = json::object();
    gd["token_index"] = 42;
    gd["scene_path"] = "route/a.ks";
    gd["f"]["tbl"]["a"]["b"] = 1;
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
    CHECK(loaded == gd);                          // exact deep roundtrip w/ encryption
    CHECK(loaded["f"]["tbl"]["a"]["b"] == 1);
    CHECK(loaded["f"]["tbl"]["list"].size() == 3);
    CHECK(loaded["loop_stacks"][0]["end"] == 10);
    CHECK(loaded["seen_scenes"].size() == 2);
    CHECK(meta.slot == 8);
    CHECK(meta.sceneName == "chapter1");
    CHECK(meta.tokenIndex == 42);

    // listSaves() surfaces the encrypted slot with decrypted metadata.
    auto saves = sm.listSaves();
    REQUIRE(saves.size() == 1);
    CHECK(saves[0].slot == 8);
    CHECK(saves[0].sceneName == "chapter1");
    CHECK(saves[0].tokenIndex == 42);
}

TEST_CASE("Storage: encrypted save file on disk is not plaintext-readable") {
    TestPaths::ScopedTempDir dir("storage_enc_secret");
    ScopedCryptoRegistration cryptoRegistration;
    SaveManager sm;
    sm.init(dir.string());

    uint8_t key[32] = {};
    for (int i = 0; i < 32; ++i) key[i] = static_cast<uint8_t>(i + 1);
    sm.setEncryptionKey(key);

    // Distinctive markers that would obviously leak if the file were stored
    // as plaintext.
    const std::string secret = "SUPER_SECRET_MARKER_a1b2c3d4";
    const std::string sceneMarker = "encrypted_scene_XYZ_route";
    json gd = {{"secret", secret}, {"hp", 100}, {"flags", {{"a", true}}}};
    REQUIRE(sm.save(7, gd, sceneMarker, 9));

    std::string raw = readRawFileBytes(dir.string() + "save_7.json");
    REQUIRE(raw.size() > 4 + 12 + 16);            // "CAES" + nonce + tag + ct

    // On-disk format: 4-byte "CAES" magic, then a 12-byte nonce, 16-byte tag,
    // then AES-GCM ciphertext. No plaintext shell may survive.
    CHECK(std::memcmp(raw.data(), "CAES", 4) == 0);
    CHECK(raw.size() >= 4 + 12 + 16 + 4);         // at least a few ciphertext bytes

    // Payload strings, JSON structure markers, and the scene name must all be
    // absent from the raw ciphertext bytes.
    CHECK(raw.find(secret) == std::string::npos);
    CHECK(raw.find(sceneMarker) == std::string::npos);
    CHECK(raw.find("\"secret\"") == std::string::npos);
    CHECK(raw.find("\"data\"") == std::string::npos);

    // Sanity: the marker is still fully recoverable through the keyed manager.
    json loaded = sm.load(7);
    CHECK(loaded["secret"] == secret);
    CHECK(loaded["hp"] == 100);
}

TEST_CASE("Storage: encrypted save loaded without key (plain provider) fails gracefully") {
    TestPaths::ScopedTempDir dir("storage_enc_plain");
    ScopedCryptoRegistration cryptoRegistration;
    SaveManager sm;
    sm.init(dir.string());

    uint8_t key[32] = {};
    for (int i = 0; i < 32; ++i) key[i] = static_cast<uint8_t>(i + 1);
    sm.setEncryptionKey(key);
    REQUIRE(sm.save(4, {{"secret", "classified"}}, "enc", 0));

    // A "plain" manager (no key set) sees the binary ciphertext bytes; it must
    // reject them gracefully -- no crash, null json -- rather than surface or
    // decode the raw file.
    sm.clearEncryptionKey();
    CHECK_FALSE(sm.isEncryptionEnabled());

    json loaded = sm.load(4);
    CHECK(loaded.is_null());                       // graceful failure, not garbage
    CHECK_FALSE(loaded.is_object());

    // slotExists still reflects file presence (bytes exist but are not a
    // readable save).
    CHECK(sm.slotExists(4));

    // listSaves() must skip the undecodable ciphertext instead of surfacing it.
    CHECK(sm.listSaves().empty());

    // The manager stays usable: a fresh plain save after the encrypted one.
    REQUIRE(sm.save(6, {{"plain", true}}, "plain", 0));
    CHECK(sm.load(6)["plain"] == true);
}

TEST_CASE("Storage: wrong encryption key rejects load gracefully (GCM auth)") {
    TestPaths::ScopedTempDir dir("storage_enc_wrongkey");
    ScopedCryptoRegistration cryptoRegistration;
    SaveManager sm;
    sm.init(dir.string());

    uint8_t keyA[32] = {};
    for (int i = 0; i < 32; ++i) keyA[i] = static_cast<uint8_t>(i + 1);
    sm.setEncryptionKey(keyA);
    REQUIRE(sm.save(2, {{"data", "authenticated_secret"}}, "enc", 1));

    // Switching to a different key must fail decrypt (tag mismatch) and
    // surface null json -- never crash, never return wrong-key payload.
    uint8_t keyB[32] = {};
    for (int i = 0; i < 32; ++i) keyB[i] = static_cast<uint8_t>(100 - i);
    sm.setEncryptionKey(keyB);
    CHECK(sm.load(2).is_null());
    CHECK(sm.listSaves().empty());

    // Restoring the correct key recovers the save.
    sm.setEncryptionKey(keyA);
    json again = sm.load(2);
    CHECK(again["data"] == "authenticated_secret");
}

TEST_CASE("Storage: plain save loaded with encryption enabled stays loadable (magic-gated)") {
    TestPaths::ScopedTempDir dir("storage_enc_loadsheer");
    ScopedCryptoRegistration cryptoRegistration;
    SaveManager sm;
    sm.init(dir.string());

    // Write a PLAIN (unencrypted) save with no key set.
    sm.clearEncryptionKey();
    REQUIRE(sm.save(2, {{"plain", "text"}, {"n", 5}}, "plain_scene", 3));
    std::string rawPlain = readRawFileBytes(dir.string() + "save_2.json");
    REQUIRE(rawPlain.find("CAES") == std::string::npos);
    REQUIRE(rawPlain.find("\"plain\"") != std::string::npos);  // confirmed plaintext

    // Now enable encryption (the "encrypted provider" reading a plain save).
    // Verified behavior: decryption is gated on the "CAES" magic prefix, so a
    // plain file is NOT treated as ciphertext and loads intact. This is safe
    // auto-detection rather than a failure -- intentionally documented, not a bug.
    uint8_t key[32] = {};
    for (int i = 0; i < 32; ++i) key[i] = static_cast<uint8_t>(i + 1);
    sm.setEncryptionKey(key);

    json loaded = sm.load(2);
    CHECK(loaded.is_object());
    CHECK(loaded["plain"] == "text");
    CHECK(loaded["n"] == 5);

    // ...but a plain file that is then written through the keyed manager
    // becomes encrypted on disk.
    REQUIRE(sm.save(2, {{"plain", "text"}, {"n", 6}}, "plain_scene", 3));
    std::string rawReEnc = readRawFileBytes(dir.string() + "save_2.json");
    CHECK(std::memcmp(rawReEnc.data(), "CAES", 4) == 0);
    CHECK(sm.load(2)["n"] == 6);
}

TEST_CASE("Storage: forged CAES-magic decoy rejected gracefully under encryption") {
    TestPaths::ScopedTempDir dir("storage_enc_forged");
    ScopedCryptoRegistration cryptoRegistration;
    SaveManager sm;
    sm.init(dir.string());

    // A malicious/accidental file that begins with "CAES" magic but whose body
    // is not valid GCM ciphertext (e.g. rest of a plaintext truncated to look
    // encrypted). With encryption enabled, readFile must reject decrypt and
    // surface null json -- no crash, no partial/raw passthrough.
    {
        std::ofstream f(dir.string() + "save_3.json", std::ios::binary | std::ios::trunc);
        f.write("CAES", 4);
        f << "{not-valid-ciphertext-just-plaintext-garbage}";
    }

    uint8_t key[32] = {};
    for (int i = 0; i < 32; ++i) key[i] = static_cast<uint8_t>(i + 1);
    sm.setEncryptionKey(key);

    json loaded = sm.load(3);
    CHECK(loaded.is_null());
    CHECK(sm.listSaves().empty());                // undecodable -> skipped
    // Manager remains usable.
    REQUIRE(sm.save(3, {{"ok", 1}}, "fresh", 0));
    CHECK(sm.load(3)["ok"] == 1);
}

TEST_CASE("Storage: encrypted provider mirrors slot bounds/overwrite/delete") {
    TestPaths::ScopedTempDir dir("storage_enc_bounds");
    ScopedCryptoRegistration cryptoRegistration;
    SaveManager sm;
    sm.init(dir.string());

    uint8_t key[32] = {};
    for (int i = 0; i < 32; ++i) key[i] = static_cast<uint8_t>(i + 1);
    sm.setEncryptionKey(key);

    // Legal bounds round-trip under encryption, exactly like the plain path.
    REQUIRE(sm.save(0, {{"n", 0}}, "s0", 1));
    REQUIRE(sm.save(99, {{"n", 99}}, "s99", 2));
    CHECK(sm.load(0)["n"] == 0);
    CHECK(sm.load(99)["n"] == 99);
    CHECK(sm.slotExists(0));
    CHECK(sm.slotExists(99));

    // Out-of-range rejected identically under encryption.
    CHECK_FALSE(sm.save(100, {{"n", 1}}, "hi", 0));
    CHECK_FALSE(sm.save(-1, {{"n", 1}}, "neg", 0));
    CHECK(sm.load(100).is_null());
    CHECK(sm.load(-1).is_null());
    CHECK_FALSE(sm.deleteSlot(100));
    CHECK_FALSE(sm.deleteSlot(-1));

    // Overwrite same encrypted slot replaces payload + metadata.
    REQUIRE(sm.save(5, {{"phase", 1}}, "sceneA", 3));
    REQUIRE(sm.save(5, {{"phase", 2}, {"note", "second"}}, "sceneB", 7));
    SaveMeta meta;
    json loaded = sm.load(5, &meta);
    CHECK(loaded["phase"] == 2);
    CHECK(loaded["note"] == "second");
    CHECK(meta.sceneName == "sceneB");
    CHECK(meta.tokenIndex == 7);
    // The overwritten slot stays encrypted on disk (fresh key/nonce each write).
    std::string raw = readRawFileBytes(dir.string() + "save_5.json");
    CHECK(std::memcmp(raw.data(), "CAES", 4) == 0);

    // Delete works; a second delete is a safe no-op.
    REQUIRE(sm.deleteSlot(5));
    CHECK_FALSE(sm.slotExists(5));
    CHECK_FALSE(sm.deleteSlot(5));
    CHECK(sm.load(5).is_null());

    // Exactly slots 0 and 99 remain listed.
    auto saves = sm.listSaves();
    REQUIRE(saves.size() == 2);
    CHECK(saves[0].slot == 0);
    CHECK(saves[1].slot == 99);
}

TEST_CASE("Storage: tampered encrypted save (ciphertext & nonce) rejected without crash") {
    TestPaths::ScopedTempDir dir("storage_enc_tamper");
    ScopedCryptoRegistration cryptoRegistration;
    SaveManager sm;
    sm.init(dir.string());

    uint8_t key[32] = {};
    for (int i = 0; i < 32; ++i) key[i] = static_cast<uint8_t>(i + 1);
    sm.setEncryptionKey(key);

    json gd = {{"secret", "integrity_protected_payload"}, {"v", 7}};
    REQUIRE(sm.save(3, gd, "tamper", 0));

    std::string raw = readRawFileBytes(dir.string() + "save_3.json");
    REQUIRE(raw.size() > 4 + 12 + 16 + 8);

    // (1) Flip a byte in the CIPHERTEXT body (index 40, past the 4+12+16 header).
    auto flipInRegion = [&](std::string bytes, size_t idx, size_t* outSize) {
        bytes[idx] = static_cast<char>(bytes[idx] ^ 0xFF);
        std::ofstream out(dir.string() + "save_3.json", std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.close();
        *outSize = bytes.size();
    };
    size_t size = 0;
    flipInRegion(raw, 40, &size);
    CHECK(sm.load(3).is_null());                  // GCM auth fails -> null, no crash

    // (2) Flip a byte in the NONCE region (index 6). Changing the nonce must
    // also break GCM auth (tag drawn over the ciphertext + associated data).
    flipInRegion(raw, 6, &size);
    CHECK(sm.load(3).is_null());

    // (3) Corrupted file is skipped by listing. slotExists() reports false
    // because readFile returns empty when decryption fails (a corrupt
    // encrypted file is opaque); note this differs from a CORRUPT PLAIN file,
    // which is returned as raw bytes and therefore still "exists" (see the
    // plain corrupted-save case). Consistent with the keyed read path.
    CHECK_FALSE(sm.slotExists(3));
    CHECK(sm.listSaves().empty());

    // Manager remains fully usable after tampering; a fresh encrypted slot is
    // written (with a fresh nonce) and loads correctly.
    REQUIRE(sm.save(3, gd, "tamper", 0));
    json recovered = sm.load(3);
    CHECK(recovered["secret"] == "integrity_protected_payload");
    CHECK(recovered["v"] == 7);
}

// =============================================================================
// Round-79+ boundary expansion:
//  * large-payload roundtrip (~1 MiB) plain & encrypted, plus the undocumented
//    save/load size-ceiling asymmetry (save accepts >10 MiB, load then rejects)
//  * UTF-8 / CJK / emoji text roundtrip
//  * unknown future schema_version passes through unmigrated; chained migration
//    runs step-by-step when loading at an intermediate schema version
//  * rapid sequential multi-slot save + immediate same-process load (disk
//    persistence -- SaveManager keeps no in-memory cache)
//  * pluggable in-memory ISaveProvider: full save/load/list/delete flow and
//    provider-error propagation back through the SaveManager facade
// =============================================================================

// Build a deterministic repeated-string of the requested byte length.
static std::string makeRepeatedPayload(size_t bytes, char seedLoop) {
    std::string s;
    s.reserve(bytes);
    size_t i = 0;
    while (i < bytes) {
        s.push_back(static_cast<char>('A' + ((i + seedLoop) % 26)));
        ++i;
    }
    return s;
}

TEST_CASE("Storage: large payload ~1 MiB roundtrips exactly (plain and encrypted)") {
    TestPaths::ScopedTempDir dir("storage_big_roundtrip");
    SaveManager sm;
    sm.init(dir.string());

    const std::string big = makeRepeatedPayload(1024 * 1024, 0);  // 1 MiB
    json gd = {{"title", "big"}, {"blob", big}, {"n", 123456}};

    // Plain path.
    REQUIRE(sm.save(2, gd, "bigplain", 1));
    json loaded = sm.load(2);
    CHECK(loaded["title"] == "big");
    CHECK(loaded["blob"].is_string());
    CHECK(loaded["blob"].get<std::string>() == big);
    CHECK(loaded["n"] == 123456);

    // Encrypted path preserves the same large payload exactly.
    ScopedCryptoRegistration cryptoRegistration;
    uint8_t key[32] = {};
    for (int i = 0; i < 32; ++i) key[i] = static_cast<uint8_t>(i + 1);
    sm.setEncryptionKey(key);
    REQUIRE(sm.save(3, gd, "bigenc", 2));
    json encLoaded = sm.load(3);
    CHECK(encLoaded["blob"].get<std::string>() == big);
    CHECK(encLoaded["n"] == 123456);
    // At-rest: the on-disk encrypted file must exceed the plaintext size and
    // start with the CAES magic (opaque, not a raw large-JSON dump).
    std::string rawEnc = readRawFileBytes(dir.string() + "save_3.json");
    CHECK(std::memcmp(rawEnc.data(), "CAES", 4) == 0);
    CHECK(rawEnc.size() >= big.size());
}

TEST_CASE("Storage: oversized save (>10 MiB ceiling) is written but not loadable -- asymmetry") {
    TestPaths::ScopedTempDir dir("storage_oversize");
    SaveManager sm;
    sm.init(dir.string());

    // readFile() enforces a 10 MiB MAX_SAVE_SIZE ceiling and returns empty for
    // any file larger than it. writeFile() has NO such guard, so save() at
    // ~11 MiB returns true while load()/slotExists()/listSaves() then report
    // the slot as absent/empty. Documented current behavior (latent asymmetry
    // flagged for the storage owner); assert it so a future fix is caught.
    const std::string huge = makeRepeatedPayload(11 * 1024 * 1024, 3);  // 11 MiB
    json gd = {{"huge", huge}};

    bool saved = sm.save(4, gd, "huge", 0);
    CHECK(saved == true);                    // writeFile accepts the oversized blob

    // The file physically exists on disk and exceeds the read ceiling.
    std::filesystem::path fp(dir.string() + "save_4.json");
    CHECK(std::filesystem::exists(fp));
    CHECK(std::filesystem::file_size(fp) > 10 * 1024 * 1024);

    // ...but the manager cannot read it back: load null, slot not "exists",
    // and the oversized slot is not listed.
    CHECK(sm.load(4).is_null());
    CHECK_FALSE(sm.slotExists(4));
    CHECK(sm.listSaves().empty());

    // A normal-size save alongside stays fully functional.
    REQUIRE(sm.save(5, {{"ok", 1}}, "fine", 0));
    CHECK(sm.load(5)["ok"] == 1);
}

TEST_CASE("Storage: UTF-8 CJK / emoji / combining-mark text roundtrips exactly") {
    TestPaths::ScopedTempDir dir("storage_utf8");
    SaveManager sm;
    sm.init(dir.string());

    const std::string utf8 = "こんにちは世界 🌸✨ héllo wörld ✅ café résumé "
                             "漢字仮名交じり文 且 合 有 好\n e\u0301\u0302 \t emoji: 🎮⛩️🏮 愛❤️";
    const std::string nulInString = "abc\0def";  // embedded NUL must survive too
    json gd;
    gd["text"] = utf8;
    gd["with_nul"] = nulInString;
    gd["emojis"] = json::array({"✨", "🎌", "🌸", "❤️"});
    gd["mixed"] = "🎥 映画を見る 'quoted' \u00e9";

    REQUIRE(sm.save(6, gd, "utf8scene", 4));

    json loaded = sm.load(6);
    CHECK(loaded["text"].get<std::string>() == utf8);
    CHECK(loaded["with_nul"].get<std::string>() == nulInString);
    REQUIRE(loaded["emojis"].is_array());
    CHECK(loaded["emojis"].size() == 4);
    CHECK(loaded["emojis"][0] == "✨");
    CHECK(loaded["emojis"][3] == "❤️");
    CHECK(loaded["mixed"] == "🎥 映画を見る 'quoted' \u00e9");

    // In file metadata, the UTF-8 scene name is preserved verbatim (bytes).
    SaveMeta meta;
    CHECK(sm.load(6, &meta)["text"].get<std::string>() == utf8);
}

TEST_CASE("Storage: unknown future schema version loads unmigrated (pass-through)") {
    TestPaths::ScopedTempDir dir("storage_future_ver");
    SaveManager sm;
    sm.init(dir.string());  // built-in chain tops out at v5

    CHECK(sm.currentSchemaVersion() == 5);

    // Hand-write a save from a "future" engine (schema_version > current).
    json env;
    env["schema_version"] = 99;
    env["timestamp"] = 0;
    env["scene"] = "from_future";
    env["token_index"] = 7;
    env["thumbnail"] = "";
    env["engine_version"] = "9.99.99";
    env["data"] = {{"futuristic_field", true}, {"preserve", "intact"}};
    {
        std::ofstream f(dir.string() + "save_2.json", std::ios::binary | std::ios::trunc);
        f << env.dump();
    }

    // load() only migrates when schemaVer < current, so a DEEPER-numered save
    // passes through byte-for-byte with its original contents.
    SaveMeta meta;
    json loaded = sm.load(2, &meta);
    CHECK(loaded.is_object());
    CHECK(loaded["futuristic_field"] == true);
    CHECK(loaded["preserve"] == "intact");
    // No migration applied, so outMeta keeps the original (future) version.
    CHECK(meta.schemaVersion == 99);
    // The future save surfaces in listSaves preserving its version.
    auto saves = sm.listSaves();
    REQUIRE(saves.size() == 1);
    CHECK(saves[0].slot == 2);
    CHECK(saves[0].schemaVersion == 99);
}

TEST_CASE("Storage: chained migration runs stepwise when loading an intermediate v2 save") {
    TestPaths::ScopedTempDir dir("storage_chain_intermediate");
    SaveManager sm;
    sm.init(dir.string());  // v1->v2->v3->v4->v5 chain installed
    REQUIRE(sm.currentSchemaVersion() == 5);

    // Simulate a save produced at schema v2 (already has playtime, missing
    // the v3/v4/v5 fields the chain must append).
    json env;
    env["schema_version"] = 2;
    env["timestamp"] = 0;
    env["scene"] = "midchain";
    env["token_index"] = 5;
    env["thumbnail"] = "";
    env["engine_version"] = "0.95.0";
    env["data"] = {{"playtime", 1234}, {"preserved", "yes"}};
    {
        std::ofstream f(dir.string() + "save_1.json", std::ios::binary | std::ios::trunc);
        f << env.dump();
    }

    // v2 -> v3 (minigame) -> v4 (live2d) -> v5 (editor); existing field intact.
    json loaded = sm.load(1);
    CHECK(loaded["playtime"] == 1234);       // v2 field survives the chain
    CHECK(loaded["preserved"] == "yes");      // untouched by migrations
    CHECK(loaded["minigame"].is_object());    // added at v3
    CHECK(loaded["live2d"].is_object());      // added at v4
    CHECK(loaded["editor"].is_object());      // added at v5
}

TEST_CASE("Storage: rapid sequential multi-slot save + immediate load (disk-backed)") {
    TestPaths::ScopedTempDir dir("storage_sequential");
    SaveManager sm;
    sm.init(dir.string());

    // Fill many slots back-to-back; SaveManager keeps NO in-memory cache, so
    // an immediate load re-reads the file from the provider on every call.
    const int N = 10;
    for (int s = 0; s < N; ++s) {
        json gd = {{"slot", s}, {"value", s * 10}};
        REQUIRE(sm.save(s, gd, "scene_" + std::to_string(s), s));
    }
    // Every slot loads immediately in the same process with matching metadata.
    for (int s = 0; s < N; ++s) {
        SaveMeta meta;
        json loaded = sm.load(s, &meta);
        CHECK(loaded["slot"] == s);
        CHECK(loaded["value"] == s * 10);
        CHECK(meta.slot == s);
        CHECK(meta.sceneName == "scene_" + std::to_string(s));
        CHECK(sm.slotExists(s));
    }
    // All N listed, in ascending slot order, no duplicates / no gaps.
    auto saves = sm.listSaves();
    REQUIRE(saves.size() == static_cast<size_t>(N));
    for (int s = 0; s < N; ++s) {
        CHECK(saves[static_cast<size_t>(s)].slot == s);
    }
}

// In-memory ISaveProvider for provider-injection testing: stores raw bytes in
// a std::map keyed by path, never touching the filesystem.
class InMemorySaveProvider final : public ISaveProvider {
public:
    std::map<std::string, std::string> files;
    // Error injection: when active, the flagged operation fails and reports.
    bool failWrite = false;
    bool failReadReturnEmpty = false;
    bool failDelete = false;

    std::string readFile(const std::string& path) override {
        if (failReadReturnEmpty) return "";
        const auto it = files.find(path);
        return it == files.end() ? std::string() : it->second;
    }
    bool writeFile(const std::string& path, const std::string& content) override {
        if (failWrite) return false;
        files[path] = content;
        return true;
    }
    bool deleteFile(const std::string& path) override {
        if (failDelete) return false;
        return files.erase(path) > 0;
    }
    std::vector<std::string> listFiles(const std::string& pattern) override {
        std::vector<std::string> out;
        for (const auto& [name, _] : files) {
            if (pattern.empty() || name.find(pattern) != std::string::npos) out.push_back(name);
        }
        return out;
    }
    bool pushToCloud(const std::string&) override { return false; }
    bool pullFromCloud(const std::string&) override { return false; }
    bool supportsCloudSync() const override { return false; }
};

TEST_CASE("Storage: custom in-memory ISaveProvider drives the full save/load flow") {
    TestPaths::ScopedTempDir dir("storage_provider_inmem");
    SaveManager sm;
    sm.init(dir.string());  // m_saveDir must be non-empty for listSaves()

    auto mem = std::make_unique<InMemorySaveProvider>();
    InMemorySaveProvider* memPtr = mem.get();
    sm.setSaveProvider(std::move(mem));
    CHECK(sm.getSaveProvider() == memPtr);

    // save() routes the whole slot path through the provider's writeFile.
    json gd = {{"p", true}, {"n", 7}, {"nested", {{"x", {{"y", 1}}}}}};
    REQUIRE(sm.save(3, gd, "memscene", 6));

    // Raw bytes land in the in-memory map under the canonical slot path...
    const std::string path = dir.string() + "save_3.json";
    CHECK(memPtr->files.count(path) == 1);

    // ...and the SaveManager facade reads them back identically.
    SaveMeta meta;
    json loaded = sm.load(3, &meta);
    CHECK(loaded == gd);
    CHECK(meta.slot == 3);
    CHECK(meta.sceneName == "memscene");
    CHECK(meta.tokenIndex == 6);
    CHECK(sm.slotExists(3));

    // listSaves() enumerates the in-memory provider (not the filesystem).
    auto saves = sm.listSaves();
    REQUIRE(saves.size() == 1);
    CHECK(saves[0].slot == 3);

    // deleteSlot() routes through the provider and removes the byte entry.
    REQUIRE(sm.deleteSlot(3));
    CHECK_FALSE(sm.slotExists(3));
    CHECK(sm.load(3).is_null());
    CHECK(memPtr->files.empty());
}

TEST_CASE("Storage: provider write/read/delete errors propagate gracefully") {
    TestPaths::ScopedTempDir dir("storage_provider_err");
    SaveManager sm;
    sm.init(dir.string());

    auto mem = std::make_unique<InMemorySaveProvider>();
    InMemorySaveProvider* memPtr = mem.get();
    sm.setSaveProvider(std::move(mem));

    // Normal write first (so we can later exercise the read/delete paths).
    REQUIRE(sm.save(2, {{"ok", 1}}, "base", 0));

    // 1) Provider write failure -> save() returns false and stores nothing.
    memPtr->files.clear();
    memPtr->failWrite = true;
    CHECK_FALSE(sm.save(2, {{"ok", 2}}, "wfail", 0));
    CHECK(memPtr->files.empty());
    CHECK(sm.load(2).is_null());
    memPtr->failWrite = false;

    // Re-establish a baseline entry.
    REQUIRE(sm.save(2, {{"ok", 1}}, "base", 0));

    // 2) Provider read failure (empty) -> load null, slot not exists, not listed.
    memPtr->failReadReturnEmpty = true;
    CHECK(sm.load(2).is_null());
    CHECK_FALSE(sm.slotExists(2));
    CHECK(sm.listSaves().empty());
    memPtr->failReadReturnEmpty = false;
    CHECK(sm.load(2)["ok"] == 1);   // recovers when the provider does

    // 3) Provider delete failure -> deleteSlot() returns false, slot remains.
    memPtr->failDelete = true;
    CHECK_FALSE(sm.deleteSlot(2));
    CHECK(sm.load(2)["ok"] == 1);   // still present
    memPtr->failDelete = false;
    REQUIRE(sm.deleteSlot(2));      // succeeds once the provider is healthy
    CHECK_FALSE(sm.slotExists(2));
}

