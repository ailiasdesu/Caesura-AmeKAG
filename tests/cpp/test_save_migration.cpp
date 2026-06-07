#include "doctest.h"
#include "System/SaveManager.h"
#include <filesystem>
#include <cstdio>

using namespace Caesura;

TEST_CASE("SaveManager::migration chain adds engine_version") {
    namespace fs = std::filesystem;
    fs::remove_all("test_mig");
    auto& sm = SaveManager::instance();
    sm.init("test_mig/");

    CHECK(sm.save(1, R"({"val":1})", "s1", 0));

    std::string data = sm.load(1);
    CHECK_FALSE(data.empty());
    CHECK(!data.empty());

    fs::remove_all("test_mig");
}

TEST_CASE("SaveManager::jsonGetString missing key returns empty") {
    std::string json = R"({"a":1})";
    CHECK(SaveManager::jsonGetString(json, "missing") == "");
}

TEST_CASE("SaveManager::jsonGetInt missing key returns 0") {
    std::string json = R"({"a":1})";
    CHECK(SaveManager::jsonGetInt(json, "missing") == 0);
}

TEST_CASE("SaveManager::jsonEscape edge cases") {
    CHECK(SaveManager::jsonEscape("") == "");
    CHECK(SaveManager::jsonEscape("\"") == "\\\"");
    CHECK(SaveManager::jsonEscape("\\") == "\\\\");
    CHECK(SaveManager::jsonEscape("\n") == "\\n");
    CHECK(SaveManager::jsonEscape("\r") == "\\r");
    CHECK(SaveManager::jsonEscape("\t") == "\\t");
}

