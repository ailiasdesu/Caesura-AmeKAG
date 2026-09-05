// test_save_roundtrip.cpp - save/load roundtrip integration tests
#include "doctest.h"
#include "storage/SaveManager.h"
#include "storage/LocalFileSaveProvider.h"
#include "entry/Engine.h"
#include "entry/EngineConfig.h"
#include "TestPaths.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <new>
#include <stdexcept>
#include <cstdio>

using namespace Caesura;

#include "di/BackendRegistry.h"
#include "archive/CryptoEngine.h"

namespace {

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

std::string readSaveBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void writeSaveBytes(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    REQUIRE(file.good());
}

std::array<uint8_t, 32> saveTestKey(uint8_t first = 1) {
    std::array<uint8_t, 32> key{};
    for (size_t i = 0; i < key.size(); ++i) key[i] = static_cast<uint8_t>(first + i);
    return key;
}

EngineConfig headlessSaveConfig() {
    EngineConfig config;
    config.headless = true;
    return config;
}

void checkSingleEncryptedEnvelope(const std::string& bytes,
                                  const std::array<uint8_t, 32>& key,
                                  const json& expectedData) {
    REQUIRE(bytes.size() > 32);
    REQUIRE(bytes.substr(0, 4) == "CAES");
    auto* crypto = BackendRegistry::instance().getCryptoEngine();
    REQUIRE(crypto != nullptr);
    const auto* raw = reinterpret_cast<const uint8_t*>(bytes.data());
    const auto plain = crypto->decrypt(raw + 32, bytes.size() - 32,
                                       key.data(), key.size(), raw + 4, 12, raw + 16, 16);
    REQUIRE_FALSE(plain.empty());
    const auto envelope = json::parse(plain.begin(), plain.end(), nullptr, false);
    REQUIRE(envelope.is_object());
    CHECK(envelope.at("data") == expectedData);
}

} // namespace

TEST_CASE("SaveManager: save -> load data integrity") {
    TestPaths::ScopedTempDir dir("roundtrip_integrity");
    SaveManager sm;
    sm.init(dir.string());
    sm.clearEncryptionKey();

    // Save complex JSON
    json data = {
        {"scene", "chapter1"},
        {"text_index", 42},
        {"flags", {{"flag_a", true}, {"flag_b", false}}},
        {"player_name", "Hero"},
        {"hp", 100}
    };
    CHECK(sm.save(1, data, "chapter1", 42));

    // Load and verify every field
    json loaded = sm.load(1);
    CHECK_FALSE(loaded.empty());
    CHECK(loaded["scene"] == "chapter1");
    CHECK(loaded["text_index"] == 42);
    CHECK(loaded["flags"]["flag_a"] == true);
    CHECK(loaded["flags"]["flag_b"] == false);
    CHECK(loaded["player_name"] == "Hero");
    CHECK(loaded["hp"] == 100);
}

TEST_CASE("SaveManager: nonexistent slot returns empty JSON") {
    TestPaths::ScopedTempDir dir("roundtrip_nonexistent");
    SaveManager sm;
    sm.init(dir.string());

    json data = sm.load(99);
    CHECK(data.empty());
}

TEST_CASE("SaveManager: listSaves returns correct slot list") {
    TestPaths::ScopedTempDir dir("roundtrip_list");
    SaveManager sm;
    sm.init(dir.string());

    // Empty initially
    auto saves = sm.listSaves();
    CHECK(saves.empty());

    // Save 3 slots
    sm.save(1, {{"n", 1}}, "s1", 0);
    sm.save(3, {{"n", 3}}, "s3", 0);
    sm.save(5, {{"n", 5}}, "s5", 0);

    saves = sm.listSaves();
    CHECK(saves.size() == 3);

    // Verify slot numbers
    bool found1 = false, found3 = false, found5 = false;
    for (const auto& s : saves) {
        if (s.slot == 1) found1 = true;
        if (s.slot == 3) found3 = true;
        if (s.slot == 5) found5 = true;
    }

    // Slot 99 (the upper bound) lists; slot 100 must NOT (audit: the
    // scan is bounded 0..99 -- a legacy/out-of-range file must not
    // enumerate)
    sm.save(99, {{"n", 99}}, "s99", 0);
    saves = sm.listSaves();
    bool found99 = false;
    for (const auto& s : saves) if (s.slot == 99) found99 = true;
    CHECK(found99);
    // Create an actual out-of-range file (review nit: the vacuous
    // check would pass without one -- this truly locks the 0..99 bound)
    {
        std::ofstream out100(dir.path() / "save_100.json");
        out100 << "{\"n\":100}";
        out100.close();
    }
    saves = sm.listSaves();
    bool found100 = false;
    for (const auto& s : saves) if (s.slot == 100) found100 = true;
    CHECK_FALSE(found100);
    CHECK(found1);
    CHECK(found3);
    CHECK(found5);
}

TEST_CASE("SaveManager: out-of-range slots rejected on all ops") {
    TestPaths::ScopedTempDir dir("roundtrip_oob");
    SaveManager sm;
    sm.init(dir.string());

    // save/load/delete must refuse slots outside [-2..99] (the system
    // slots -1 quicksave / -2 autosave are legal since 2026-08-24 and map
    // to dedicated files; see the system-slots test case below).
    CHECK_FALSE(sm.save(100, {{"n", 1}}, "s", 0));
    CHECK_FALSE(sm.save(-3, {{"n", 1}}, "s", 0));
    CHECK(sm.load(100).empty());
    CHECK(sm.load(-3).empty());
    CHECK_FALSE(sm.deleteSlot(100));
    CHECK_FALSE(sm.deleteSlot(-3));
    // the filesystem stayed clean
    CHECK_FALSE(std::filesystem::exists(dir.path() / "save_100.json"));
    CHECK_FALSE(std::filesystem::exists(dir.path() / "save_-3.json"));
}

TEST_CASE("SaveManager: deleteSlot removes save") {
    TestPaths::ScopedTempDir dir("roundtrip_delete");
    SaveManager sm;
    sm.init(dir.string());

    sm.save(1, {{"data", "test"}}, "test", 0);
    CHECK(sm.slotExists(1));

    CHECK(sm.deleteSlot(1));
    CHECK_FALSE(sm.slotExists(1));

    // Deleting already-deleted slot returns false or no-op
    CHECK_FALSE(sm.deleteSlot(1));
}

TEST_CASE("SaveManager: encryption roundtrip preserves data") {
    TestPaths::ScopedTempDir dir("roundtrip_encryption");
    ScopedCryptoRegistration cryptoRegistration;
    SaveManager sm;
    sm.init(dir.string());

    // Set a test encryption key (32 bytes)
    uint8_t key[32] = {};
    for (int i = 0; i < 32; ++i) key[i] = static_cast<uint8_t>(i + 1);
    sm.setEncryptionKey(key);
    CHECK(sm.isEncryptionEnabled());

    json original = {
        {"secret", "classified_info"},
        {"value", 12345}
    };
    CHECK(sm.save(1, original, "enc_test", 0));

    // Load with same key
    json decrypted = sm.load(1);
    CHECK_FALSE(decrypted.empty());
    CHECK(decrypted["secret"] == "classified_info");
    CHECK(decrypted["value"] == 12345);
}

TEST_CASE("SaveManager: tampered ciphertext rejected (GCM auth)") {
    TestPaths::ScopedTempDir dir("roundtrip_tamper");
    ScopedCryptoRegistration cryptoRegistration;
    SaveManager sm;
    sm.init(dir.string());
    uint8_t key[32] = {};
    for (int i = 0; i < 32; ++i) key[i] = static_cast<uint8_t>(i + 1);
    sm.setEncryptionKey(key);

    json original = {{"secret", "classified_info"}};
    REQUIRE(sm.save(1, original, "tamper_test", 0));

    // Flip a byte inside the GCM tag region (index 30 = 4 CAES + 12
    // nonce + 14 -- bytes 16..31 are the tag)
    std::filesystem::path p = dir.path() / "save_1.json";
    std::ifstream in(p, std::ios::binary);
    std::string data((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    in.close();
    REQUIRE(data.size() > 40);  // "CAES" + 12 nonce + 16 tag + payload
    data[30] = static_cast<char>(data[30] ^ 0xFF);  // flip a tag/cipher byte

    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.close();

    // GCM authentication must reject the tampered save
    json tampered = sm.load(1);
    CHECK(tampered.empty());
}

TEST_CASE("SaveManager: wrong key returns empty JSON") {
    TestPaths::ScopedTempDir dir("roundtrip_wrong_key");
    ScopedCryptoRegistration cryptoRegistration;
    SaveManager sm;
    sm.init(dir.string());

    // Save with key A
    uint8_t keyA[32] = {};
    for (int i = 0; i < 32; ++i) keyA[i] = static_cast<uint8_t>(i + 1);
    sm.setEncryptionKey(keyA);
    sm.save(1, {{"data", "encrypted"}}, "enc_test", 0);

    // Switch to key B
    uint8_t keyB[32] = {};
    for (int i = 0; i < 32; ++i) keyB[i] = static_cast<uint8_t>(100 - i);
    sm.setEncryptionKey(keyB);

    // Load should fail (decrypt with wrong key)
    json loaded = sm.load(1);
    CHECK(loaded.empty());

    // Switch back to key A
    sm.setEncryptionKey(keyA);
    json loaded2 = sm.load(1);
    CHECK_FALSE(loaded2.empty());
    CHECK(loaded2["data"] == "encrypted");
}

TEST_CASE("SaveManager: save without encryption when key is cleared") {
    TestPaths::ScopedTempDir dir("roundtrip_plain");
    SaveManager sm;
    sm.init(dir.string());
    sm.clearEncryptionKey();
    CHECK_FALSE(sm.isEncryptionEnabled());

    json data = {{"plain", "text"}};
    CHECK(sm.save(1, data, "plain_test", 0));

    json loaded = sm.load(1);
    CHECK_FALSE(loaded.empty());
    CHECK(loaded["plain"] == "text");

    alignas(SaveManager) unsigned char storage[sizeof(SaveManager)];
    std::fill(storage, storage + sizeof(storage), 0xA5);
    auto* scopedManager = ::new (storage) SaveManager();

    std::array<uint8_t, 32> transientKey{};
    for (size_t i = 0; i < transientKey.size(); ++i) {
        transientKey[i] = static_cast<uint8_t>(0x40 + i);
    }
    scopedManager->setEncryptionKey(transientKey.data());

    const auto keyInObjectStorage = [&]() {
        return std::search(storage, storage + sizeof(storage),
                           transientKey.begin(), transientKey.end()) != storage + sizeof(storage);
    };
    REQUIRE(keyInObjectStorage());

    scopedManager->~SaveManager();
    CHECK_FALSE(keyInObjectStorage());
}

TEST_CASE("SaveManager: multiple save/load cycles do not leak or corrupt") {
    TestPaths::ScopedTempDir dir("roundtrip_cycles");
    SaveManager sm;
    sm.init(dir.string());

    for (int i = 0; i < 10; ++i) {
        json data = {{"iteration", i}, {"message", "cycle_" + std::to_string(i)}};
        CHECK(sm.save(i % 3, data, "cycle", i));

        json loaded = sm.load(i % 3);
        CHECK_FALSE(loaded.empty());
        CHECK(loaded["iteration"] == i);
    }

    // Verify all slots exist
    CHECK(sm.slotExists(0));
    CHECK(sm.slotExists(1));
    CHECK(sm.slotExists(2));
}

TEST_CASE("SaveManager: system slots quicksave (-1) / autosave (-2) roundtrip") {
    TestPaths::ScopedTempDir dir("roundtrip_system_slots");
    SaveManager sm;
    sm.init(dir.string());

    json quick = {{"scene", "chapter1"}, {"note", "quick"}};
    json auto_ = {{"scene", "chapter1"}, {"note", "autosave"}};

    // System slots save/load like normal slots.
    CHECK(sm.save(-1, quick, "chapter1", 7));
    CHECK(sm.save(-2, auto_, "chapter1", 9));
    CHECK(sm.slotExists(-1));
    CHECK(sm.slotExists(-2));

    json loadedQuick = sm.load(-1);
    json loadedAuto  = sm.load(-2);
    CHECK_FALSE(loadedQuick.empty());
    CHECK(loadedQuick["note"] == "quick");
    CHECK_FALSE(loadedAuto.empty());
    CHECK(loadedAuto["note"] == "autosave");

    // Dedicated files land outside the 0..99 naming scheme.
    CHECK(std::filesystem::exists(dir.string() + "/save_quick.json"));
    CHECK(std::filesystem::exists(dir.string() + "/save_auto.json"));

    // The save menu enumerates 0..99 only: system slots never appear there.
    auto metas = sm.listSaves();
    for (const auto& m : metas) {
        CHECK(m.slot >= 0);
        CHECK(m.slot <= 99);
    }

    // Boundaries: -3 and 100 rejected.
    CHECK_FALSE(sm.save(-3, quick, "s", 0));
    CHECK_FALSE(sm.slotExists(-3));
    CHECK_FALSE(sm.save(100, quick, "s", 0));

    // Delete works for system slots too.
    CHECK(sm.deleteSlot(-2));
    CHECK_FALSE(sm.slotExists(-2));
    CHECK(sm.slotExists(-1));
}

TEST_CASE("U4: Engine default save provider encrypts bytes and reloads after restart") {
    TestPaths::ScopedTempDir dir("engine_default_provider_encryption");
    const auto key = saveTestKey();
    const json data = {{"secret", "U4_ENGINE_PRIVATE_PAYLOAD"}, {"route", "sun"}};
    const auto path = dir.path() / "save_4.json";

    {
        Engine engine(headlessSaveConfig());
        REQUIRE(engine.init());
        auto* saves = BackendRegistry::instance().getSaveManager();
        REQUIRE(saves != nullptr);
        // Keep the actual provider installed by Engine::init(); only isolate its directory.
        REQUIRE(dynamic_cast<LocalFileSaveProvider*>(saves->getSaveProvider()) != nullptr);
        saves->init(dir.string());
        saves->setEncryptionKey(key.data());
        REQUIRE(saves->save(4, data, "U4_PRIVATE_SCENE", 17));
        const auto bytes = readSaveBytes(path);
        CHECK(bytes.substr(0, 4) == "CAES");
        CHECK(bytes.find("U4_ENGINE_PRIVATE_PAYLOAD") == std::string::npos);
        CHECK(bytes.find("U4_PRIVATE_SCENE") == std::string::npos);
    }

    const auto original = readSaveBytes(path);
    {
        Engine engine(headlessSaveConfig());
        REQUIRE(engine.init());
        auto* saves = BackendRegistry::instance().getSaveManager();
        REQUIRE(saves != nullptr);
        saves->init(dir.string());
        SaveMeta meta;
        meta.sceneName = "unchanged";
        meta.tokenIndex = 987;
        CHECK(saves->load(4, &meta).is_null());
        CHECK(meta.sceneName == "unchanged");
        CHECK(meta.tokenIndex == 987);

        const auto wrongKey = saveTestKey(80);
        saves->setEncryptionKey(wrongKey.data());
        CHECK(saves->load(4, &meta).is_null());
        CHECK(meta.sceneName == "unchanged");
        saves->setEncryptionKey(key.data());
        CHECK(saves->load(4, &meta) == data);
        CHECK(meta.sceneName == "U4_PRIVATE_SCENE");
        CHECK(meta.tokenIndex == 17);
        CHECK(saves->slotExists(4));
        REQUIRE(saves->listSaves().size() == 1);
        CHECK(saves->listSaves().front().slot == 4);
        CHECK(readSaveBytes(path) == original);
        checkSingleEncryptedEnvelope(original, key, data);
    }
}

TEST_CASE("U4: Local provider keeps legacy plaintext unchanged until explicit save") {
    TestPaths::ScopedTempDir dir("provider_plaintext_compatibility");
    ScopedCryptoRegistration crypto;
    SaveManager saves;
    saves.init(dir.string());
    saves.setSaveProvider(std::make_unique<LocalFileSaveProvider>());
    const json data = {{"legacy", "U4_PLAINTEXT_IMPORT"}, {"hp", 12}};
    REQUIRE(saves.save(1, data, "legacy", 8));
    const auto path = dir.path() / "save_1.json";
    const auto original = readSaveBytes(path);
    REQUIRE(original.find("U4_PLAINTEXT_IMPORT") != std::string::npos);

    const auto key = saveTestKey();
    saves.setEncryptionKey(key.data());
    CHECK(saves.load(1) == data);
    CHECK(readSaveBytes(path) == original);
    REQUIRE(saves.save(1, data, "legacy", 8));
    const auto encrypted = readSaveBytes(path);
    CHECK(encrypted != original);
    CHECK(encrypted.find("U4_PLAINTEXT_IMPORT") == std::string::npos);
    checkSingleEncryptedEnvelope(encrypted, key, data);
}

TEST_CASE("U4: Local provider reads existing CAES and encrypts only once on resave") {
    TestPaths::ScopedTempDir dir("provider_existing_caes");
    ScopedCryptoRegistration crypto;
    const auto key = saveTestKey();
    const json data = {{"marker", "U4_PRE_PROVIDER_CAES"}, {"nested", {{"flag", true}}}};
    {
        // Produce the established CAES format through the previously working no-provider path.
        SaveManager legacy;
        legacy.init(dir.string());
        legacy.setEncryptionKey(key.data());
        REQUIRE(legacy.save(2, data, "legacy-caes", 3));
    }
    const auto path = dir.path() / "save_2.json";
    const auto original = readSaveBytes(path);
    REQUIRE(original.substr(0, 4) == "CAES");

    SaveManager saves;
    saves.init(dir.string());
    saves.setSaveProvider(std::make_unique<LocalFileSaveProvider>());
    saves.setEncryptionKey(key.data());
    CHECK(saves.load(2) == data);
    CHECK(readSaveBytes(path) == original);
    REQUIRE(saves.save(2, data, "legacy-caes", 3));
    checkSingleEncryptedEnvelope(readSaveBytes(path), key, data);
}

TEST_CASE("U4: Local provider rejects malformed CAES without changing load metadata") {
    TestPaths::ScopedTempDir dir("provider_malformed_caes");
    ScopedCryptoRegistration crypto;
    const auto key = saveTestKey();
    SaveManager legacy;
    legacy.init(dir.string());
    legacy.setEncryptionKey(key.data());
    REQUIRE(legacy.save(6, {{"secret", "U4_AUTHENTICATED"}}, "original", 4));
    const auto path = dir.path() / "save_6.json";
    const auto original = readSaveBytes(path);
    REQUIRE(original.size() > 32);

    std::vector<std::string> malformed = {"CAES", original.substr(0, 31), original.substr(0, 32),
        "CAES" + json({{"data", {{"forged", true}}}}).dump()};
    for (const size_t offset : {size_t{0}, size_t{4}, size_t{16}, size_t{32}}) {
        auto changed = original;
        changed[offset] = static_cast<char>(changed[offset] ^ 0x40);
        malformed.push_back(std::move(changed));
    }
    SaveManager saves;
    saves.init(dir.string());
    saves.setSaveProvider(std::make_unique<LocalFileSaveProvider>());
    saves.setEncryptionKey(key.data());
    for (size_t i = 0; i < malformed.size(); ++i) {
        CAPTURE(i);
        writeSaveBytes(path, malformed[i]);
        SaveMeta meta;
        meta.sceneName = "unchanged";
        meta.tokenIndex = 987;
        CHECK(saves.load(6, &meta).is_null());
        CHECK(meta.sceneName == "unchanged");
        CHECK(meta.tokenIndex == 987);
        CHECK(saves.listSaves().empty());
        CHECK(saves.loadLegacyPlaintext(6, &meta).is_null());
        CHECK(meta.sceneName == "unchanged");
        CHECK(meta.tokenIndex == 987);
        CHECK(readSaveBytes(path) == malformed[i]);
    }
}

TEST_CASE("U4: require-encrypted preserves existing bytes without a key and after clear") {
    TestPaths::ScopedTempDir dir("required_encryption_key_lifecycle");
    ScopedCryptoRegistration crypto;
    SaveManager saves;
    saves.init(dir.string());
    SUBCASE("LocalFile provider") { saves.setSaveProvider(std::make_unique<LocalFileSaveProvider>()); }
    SUBCASE("legacy file path") { CHECK(saves.getSaveProvider() == nullptr); }
    const json oldData = {{"route", "original"}};
    REQUIRE(saves.save(1, oldData, "legacy", 6));
    const auto path = dir.path() / "save_1.json";
    const auto original = readSaveBytes(path);
    CHECK(saves.getEncryptionPolicy() == SaveEncryptionPolicy::Compatible);
    saves.setEncryptionPolicy(SaveEncryptionPolicy::RequireEncrypted);
    CHECK_FALSE(saves.save(1, {{"route", "must-not-replace"}}, "other", 7));
    CHECK(readSaveBytes(path) == original);
    CHECK(saves.load(1).is_null());

    const auto key = saveTestKey();
    saves.setEncryptionKey(key.data());
    REQUIRE(saves.save(2, oldData, "encrypted", 6));
    const auto encryptedPath = dir.path() / "save_2.json";
    const auto encrypted = readSaveBytes(encryptedPath);
    checkSingleEncryptedEnvelope(encrypted, key, oldData);
    saves.clearEncryptionKey();
    CHECK(saves.getEncryptionPolicy() == SaveEncryptionPolicy::RequireEncrypted);
    CHECK_FALSE(saves.save(2, {{"route", "must-not-replace"}}, "other", 7));
    CHECK(readSaveBytes(encryptedPath) == encrypted);
    CHECK(saves.load(2).is_null());
    saves.setEncryptionKey(key.data());
    CHECK(saves.load(2) == oldData);
}

TEST_CASE("U4: strict legacy plaintext import is explicit read-only and policy preserving") {
    TestPaths::ScopedTempDir dir("explicit_legacy_import");
    ScopedCryptoRegistration crypto;
    SaveManager saves;
    saves.init(dir.string());
    saves.setSaveProvider(std::make_unique<LocalFileSaveProvider>());
    const json oldData = {{"route", "legacy-import"}};
    REQUIRE(saves.save(1, oldData, "legacy", 6));
    const auto path = dir.path() / "save_1.json";
    const auto original = readSaveBytes(path);
    saves.setEncryptionPolicy(SaveEncryptionPolicy::RequireEncrypted);
    SaveMeta meta;
    meta.sceneName = "unchanged";
    CHECK(saves.load(1, &meta).is_null());
    CHECK(meta.sceneName == "unchanged");
    CHECK(saves.listSaves().empty());
    CHECK(saves.loadLegacyPlaintext(1, &meta) == oldData);
    CHECK(meta.sceneName == "legacy");
    CHECK(readSaveBytes(path) == original);
    CHECK(saves.getEncryptionPolicy() == SaveEncryptionPolicy::RequireEncrypted);
    CHECK_FALSE(saves.isEncryptionEnabled());
    CHECK_FALSE(saves.save(2, oldData, "imported", 6));
    CHECK_FALSE(std::filesystem::exists(dir.path() / "save_2.json"));

    const auto key = saveTestKey();
    saves.setEncryptionKey(key.data());
    REQUIRE(saves.save(2, oldData, "imported", 6));
    CHECK(readSaveBytes(path) == original); // The source is preserved; migration is caller-directed.
    checkSingleEncryptedEnvelope(readSaveBytes(dir.path() / "save_2.json"), key, oldData);
    CHECK(saves.load(2) == oldData);
    meta.sceneName = "unchanged";
    CHECK(saves.loadLegacyPlaintext(2, &meta).is_null());
    CHECK(meta.sceneName == "unchanged");
    saves.clearEncryptionKey();
    CHECK(saves.loadLegacyPlaintext(2, &meta).is_null());
    CHECK(meta.sceneName == "unchanged");
}

TEST_CASE("U4: whole-file plaintext replacement is compatible-only") {
    TestPaths::ScopedTempDir dir("plaintext_replacement_policy");
    ScopedCryptoRegistration crypto;
    SaveManager saves;
    saves.init(dir.string());
    saves.setSaveProvider(std::make_unique<LocalFileSaveProvider>());
    const json replacement = {{"route", "plaintext-replacement"}};
    REQUIRE(saves.save(1, replacement, "plain", 3));
    const auto path = dir.path() / "save_1.json";
    const auto validPlaintext = readSaveBytes(path);
    const auto key = saveTestKey();
    saves.setEncryptionKey(key.data());
    REQUIRE(saves.save(1, {{"route", "encrypted"}}, "encrypted", 9));
    REQUIRE(readSaveBytes(path).substr(0, 4) == "CAES");
    writeSaveBytes(path, validPlaintext);
    CHECK(saves.load(1) == replacement); // Compatibility is not protection from complete replacement.
    saves.setEncryptionPolicy(SaveEncryptionPolicy::RequireEncrypted);
    SaveMeta meta;
    meta.sceneName = "unchanged";
    CHECK(saves.load(1, &meta).is_null());
    CHECK(meta.sceneName == "unchanged");
    CHECK(saves.listSaves().empty());
    CHECK(readSaveBytes(path) == validPlaintext);
}

TEST_CASE("U4: keyed provider writes fail before touching the slot when crypto is missing") {
    TestPaths::ScopedTempDir dir("missing_crypto_provider_write");
    SaveManager saves;
    saves.init(dir.string());
    saves.setSaveProvider(std::make_unique<LocalFileSaveProvider>());
    REQUIRE(saves.save(1, {{"route", "original"}}, "legacy", 3));
    const auto path = dir.path() / "save_1.json";
    const auto original = readSaveBytes(path);
    const auto key = saveTestKey();
    saves.setEncryptionKey(key.data());
    struct RestoreCrypto {
        carc::ICryptoEngine* previous = BackendRegistry::instance().getCryptoEngine();
        ~RestoreCrypto() { BackendRegistry::instance().setCryptoEngine(previous); }
    } restore;
    BackendRegistry::instance().setCryptoEngine(nullptr);
    CHECK_FALSE(saves.save(1, {{"route", "must-not-replace"}}, "other", 4));
    CHECK(readSaveBytes(path) == original);
    CHECK_FALSE(std::filesystem::exists(path.string() + ".tmp"));
}

TEST_CASE("U4: unsuccessful decoded loads preserve every metadata field") {
    TestPaths::ScopedTempDir dir("load_failure_metadata");
    SaveManager saves;
    saves.init(dir.string());
    saves.setSaveProvider(std::make_unique<LocalFileSaveProvider>());
    json envelope = {{"schema_version", saves.currentSchemaVersion()}, {"scene", "replacement"},
                     {"timestamp", 123}, {"token_index", 2}, {"thumbnail", "replacement"}};
    SUBCASE("missing data") {}
    SUBCASE("null data") { envelope["data"] = nullptr; }
    SUBCASE("migration returns null") {
        const auto version = saves.currentSchemaVersion();
        envelope["data"] = {{"old", true}};
        saves.registerMigration(version, version + 1, [](json) { return json(); });
    }
    SUBCASE("migration throws") {
        const auto version = saves.currentSchemaVersion();
        envelope["data"] = {{"old", true}};
        saves.registerMigration(version, version + 1, [](json) -> json {
            throw std::runtime_error("controlled migration failure");
        });
    }
    writeSaveBytes(dir.path() / "save_1.json", envelope.dump());
    SaveMeta meta;
    meta.slot = 91;
    meta.timestamp = 456;
    meta.sceneName = "sentinel-scene";
    meta.thumbnail = "sentinel-thumbnail";
    meta.tokenIndex = 789;
    meta.schemaVersion = 90;
    CHECK(saves.load(1, &meta).is_null());
    CHECK(meta.slot == 91);
    CHECK(meta.timestamp == 456);
    CHECK(meta.sceneName == "sentinel-scene");
    CHECK(meta.thumbnail == "sentinel-thumbnail");
    CHECK(meta.tokenIndex == 789);
    CHECK(meta.schemaVersion == 90);
}

TEST_CASE("U4: valid empty object and array save data remain loadable") {
    TestPaths::ScopedTempDir dir("load_empty_containers");
    SaveManager saves;
    saves.init(dir.string());
    saves.setSaveProvider(std::make_unique<LocalFileSaveProvider>());
    for (const auto& data : {json::object(), json::array()}) {
        REQUIRE(saves.save(1, data, "empty-container", 4));
        SaveMeta meta;
        const auto loaded = saves.load(1, &meta);
        CHECK_FALSE(loaded.is_null());
        CHECK(loaded == data);
        CHECK(meta.sceneName == "empty-container");
        CHECK(meta.tokenIndex == 4);
    }
}
