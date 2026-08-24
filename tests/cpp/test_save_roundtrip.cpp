// test_save_roundtrip.cpp - save/load roundtrip integration tests
#include "doctest.h"
#include "storage/SaveManager.h"
#include "TestPaths.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <new>
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
