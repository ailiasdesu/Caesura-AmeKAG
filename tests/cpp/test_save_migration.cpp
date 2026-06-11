#include "doctest.h"
#include "System/SaveManager.h"
#include <filesystem>
#include <cstdio>
#include <fstream>

using namespace Caesura;

TEST_CASE("SaveManager::migration chain — v1 to v2 adds playtime") {
    namespace fs = std::filesystem;
    fs::remove_all("test_mig");
    auto& sm = SaveManager::instance();
    sm.init("test_mig/");

    json data = {{"val", 1}};
    CHECK(sm.save(1, data, "s1", 0));

    json loaded = sm.load(1);
    CHECK_FALSE(loaded.empty());
    // v1->v2 migration adds playtime field
    CHECK(loaded.contains("playtime"));
    CHECK(loaded["playtime"] == 0);

    fs::remove_all("test_mig");
}

TEST_CASE("SaveManager::json nil round-trip") {
    namespace fs = std::filesystem;
    fs::remove_all("test_mig2");
    auto& sm = SaveManager::instance();
    sm.init("test_mig2/");

    json data = {{"nil_val", nullptr}, {"str_val", "hello"}};
    sm.save(1, data, "nil_test", 0);

    json loaded = sm.load(1);
    CHECK(loaded["nil_val"] == nullptr);
    CHECK(loaded["str_val"] == "hello");

    fs::remove_all("test_mig2");
}

TEST_CASE("SaveManager::schema version is tracked") {
    CHECK(SaveManager::instance().currentSchemaVersion() >= 2);
}

TEST_CASE("SaveManager::JSON parse error returns empty") {
    namespace fs = std::filesystem;
    fs::remove_all("test_parse_err");
    auto& sm = SaveManager::instance();
    sm.init("test_parse_err/");

    // Write invalid JSON manually
    std::ofstream out("test_parse_err/save_0.json");
    out << "this is not json {{{";
    out.close();

    json data = sm.load(0);
    CHECK(data.empty());

    fs::remove_all("test_parse_err");
}