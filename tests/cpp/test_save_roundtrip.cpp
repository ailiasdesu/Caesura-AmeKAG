// test_save_roundtrip.cpp - save/load roundtrip integration tests
#include "doctest.h"
#include "storage/SaveManager.h"
#include <filesystem>
#include <cstdio>

using namespace Caesura;

static const char* TEST_DIR = "test_roundtrip/";

static void cleanTestDir() {
    std::filesystem::remove_all(TEST_DIR);
}

TEST_CASE("SaveManager: save -> load data integrity") {
    cleanTestDir();
    auto& sm = SaveManager::instance();
    sm.init(TEST_DIR);
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

    cleanTestDir();
}

TEST_CASE("SaveManager: nonexistent slot returns empty JSON") {
    cleanTestDir();
    auto& sm = SaveManager::instance();
    sm.init(TEST_DIR);

    json data = sm.load(99);
    CHECK(data.empty());

    cleanTestDir();
}

TEST_CASE("SaveManager: listSaves returns correct slot list") {
    cleanTestDir();
    auto& sm = SaveManager::instance();
    sm.init(TEST_DIR);

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
    CHECK(found1);
    CHECK(found3);
    CHECK(found5);

    cleanTestDir();
}

TEST_CASE("SaveManager: deleteSlot removes save") {
    cleanTestDir();
    auto& sm = SaveManager::instance();
    sm.init(TEST_DIR);

    sm.save(1, {{"data", "test"}}, "test", 0);
    CHECK(sm.slotExists(1));

    CHECK(sm.deleteSlot(1));
    CHECK_FALSE(sm.slotExists(1));

    // Deleting already-deleted slot returns false or no-op
    CHECK_FALSE(sm.deleteSlot(1));

    cleanTestDir();
}

TEST_CASE("SaveManager: encryption roundtrip preserves data") {
    cleanTestDir();
    auto& sm = SaveManager::instance();
    sm.init(TEST_DIR);

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

    cleanTestDir();
}

TEST_CASE("SaveManager: wrong key returns empty JSON") {
    cleanTestDir();
    auto& sm = SaveManager::instance();
    sm.init(TEST_DIR);

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

    cleanTestDir();
}

TEST_CASE("SaveManager: save without encryption when key is cleared") {
    cleanTestDir();
    auto& sm = SaveManager::instance();
    sm.init(TEST_DIR);
    sm.clearEncryptionKey();
    CHECK_FALSE(sm.isEncryptionEnabled());

    json data = {{"plain", "text"}};
    CHECK(sm.save(1, data, "plain_test", 0));

    json loaded = sm.load(1);
    CHECK_FALSE(loaded.empty());
    CHECK(loaded["plain"] == "text");

    cleanTestDir();
}

TEST_CASE("SaveManager: multiple save/load cycles do not leak or corrupt") {
    cleanTestDir();
    auto& sm = SaveManager::instance();
    sm.init(TEST_DIR);

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

    cleanTestDir();
}
