#include "doctest.h"
#include "storage/SaveManager.h"
#include <cstdio>
#include <cstring>
#include <filesystem>

using namespace Caesura;

TEST_CASE("SaveManager::singleton") {
    auto& a = SaveManager::instance();
    auto& b = SaveManager::instance();
    CHECK(&a == &b);
}

TEST_CASE("SaveManager::init creates save directory") {
    namespace fs = std::filesystem;
    fs::remove_all("test_saves");
    auto& sm = SaveManager::instance();
    sm.init("test_saves/");
    CHECK(fs::exists("test_saves"));
    fs::remove_all("test_saves");
}

TEST_CASE("SaveManager::save and load round-trip") {
    namespace fs = std::filesystem;
    fs::remove_all("test_saves");
    auto& sm = SaveManager::instance();
    sm.init("test_saves/");

    json data = {{"key", "value"}, {"nested", {{"a", 1}}}};
    CHECK(sm.save(1, data, "scene1", 42));
    CHECK(sm.slotExists(1));

    json loaded = sm.load(1);
    CHECK_FALSE(loaded.empty());
    CHECK(loaded["key"] == "value");
    CHECK(loaded["nested"]["a"] == 1);

    fs::remove_all("test_saves");
}

TEST_CASE("SaveManager::load nonexistent slot returns empty") {
    namespace fs = std::filesystem;
    fs::remove_all("test_saves");
    auto& sm = SaveManager::instance();
    sm.init("test_saves/");
    CHECK(sm.load(99).empty());
    CHECK_FALSE(sm.slotExists(99));
    fs::remove_all("test_saves");
}

TEST_CASE("SaveManager::listSaves") {
    namespace fs = std::filesystem;
    fs::remove_all("test_saves");
    auto& sm = SaveManager::instance();
    sm.init("test_saves/");

    sm.save(1, {{"name", "one"}}, "s1", 0);
    sm.save(3, {{"name", "three"}}, "s3", 0);
    sm.save(5, {{"name", "five"}}, "s5", 0);

    auto saves = sm.listSaves();
    CHECK(saves.size() == 3);

    fs::remove_all("test_saves");
}

TEST_CASE("SaveManager::deleteSlot") {
    namespace fs = std::filesystem;
    fs::remove_all("test_saves");
    auto& sm = SaveManager::instance();
    sm.init("test_saves/");

    sm.save(1, {{"name", "one"}}, "s1", 0);
    CHECK(sm.slotExists(1));
    sm.deleteSlot(1);
    CHECK_FALSE(sm.slotExists(1));
    CHECK(sm.load(1).empty());

    fs::remove_all("test_saves");
}

TEST_CASE("SaveManager::load with metadata") {
    namespace fs = std::filesystem;
    fs::remove_all("test_saves");
    auto& sm = SaveManager::instance();
    sm.init("test_saves/");

    sm.save(1, {{"text", "hello"}}, "MyScene", 100);

    SaveMeta meta;
    json data = sm.load(1, &meta);
    CHECK_FALSE(data.empty());
    CHECK(meta.slot == 1);
    CHECK(meta.sceneName == "MyScene");
    CHECK(meta.tokenIndex == 100);

    fs::remove_all("test_saves");
}

TEST_CASE("SaveManager::json round-trip preserves types") {
    namespace fs = std::filesystem;
    fs::remove_all("test_saves");
    auto& sm = SaveManager::instance();
    sm.init("test_saves/");

    json original = {
        {"bool_true", true},
        {"bool_false", false},
        {"int_val", 42},
        {"float_val", 3.14},
        {"string_val", "hello world"},
        {"null_val", nullptr},
        {"array_val", {1, 2, 3}},
        {"nested", {{"a", 1}, {"b", "text"}}}
    };

    sm.save(1, original, "types_test", 0);
    json loaded = sm.load(1);

    CHECK(loaded["bool_true"] == true);
    CHECK(loaded["bool_false"] == false);
    CHECK(loaded["int_val"] == 42);
    CHECK(loaded["float_val"] == 3.14);
    CHECK(loaded["string_val"] == "hello world");
    CHECK(loaded["null_val"] == nullptr);
    CHECK(loaded["array_val"].size() == 3);
    CHECK(loaded["nested"]["a"] == 1);

    fs::remove_all("test_saves");
}

TEST_CASE("SaveManager::ENGINE_VERSION is set") {
    CHECK(strlen(SaveManager::ENGINE_VERSION) > 0);
}