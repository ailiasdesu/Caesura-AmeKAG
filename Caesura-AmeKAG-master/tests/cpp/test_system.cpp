#include "doctest.h"
#include "System/SaveManager.h"
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
    
    CHECK(sm.save(1, R"({"key":"value","nested":{"a":1}})", "scene1", 42));
    CHECK(sm.slotExists(1));
    
    std::string data = sm.load(1);
    CHECK_FALSE(data.empty());
    CHECK(data.find("key") != std::string::npos);
    CHECK(data.find("value") != std::string::npos);
    
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
    
    sm.save(1, R"({"name":"one"})", "s1", 0);
    sm.save(3, R"({"name":"three"})", "s3", 0);
    sm.save(5, R"({"name":"five"})", "s5", 0);
    
    auto saves = sm.listSaves();
    CHECK(saves.size() == 3);
    
    fs::remove_all("test_saves");
}

TEST_CASE("SaveManager::deleteSlot") {
    namespace fs = std::filesystem;
    fs::remove_all("test_saves");
    auto& sm = SaveManager::instance();
    sm.init("test_saves/");
    
    sm.save(1, R"({"name":"one"})", "s1", 0);
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
    
    sm.save(1, R"({"text":"hello"})", "MyScene", 100);
    
    SaveMeta meta;
    std::string data = sm.load(1, &meta);
    CHECK_FALSE(data.empty());
    CHECK(meta.slot == 1);
    CHECK(meta.sceneName == "MyScene");
    CHECK(meta.tokenIndex == 100);
    
    fs::remove_all("test_saves");
}

TEST_CASE("SaveManager::jsonEscape") {
    CHECK(SaveManager::jsonEscape("hello") == "hello");
    CHECK(SaveManager::jsonEscape("a\"b") == "a\\\"b");
    CHECK(SaveManager::jsonEscape("a\\b") == "a\\\\b");
}

TEST_CASE("SaveManager::jsonGet helpers") {
    std::string json = R"({"name":"test","count":42})";
    CHECK(SaveManager::jsonGetString(json, "name") == "test");
    CHECK(SaveManager::jsonGetInt(json, "count") == 42);
}

TEST_CASE("SaveManager::ENGINE_VERSION is set") {
    CHECK(strlen(SaveManager::ENGINE_VERSION) > 0);
}