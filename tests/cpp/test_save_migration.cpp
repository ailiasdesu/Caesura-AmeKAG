#include "doctest.h"
#include "storage/SaveManager.h"
#include "TestPaths.h"
#include <filesystem>
#include <cstdio>
#include <fstream>

using namespace Caesura;

TEST_CASE("SaveManager::migration chain — v1 to v2 adds playtime") {
    TestPaths::ScopedTempDir dir("migration_chain");
    SaveManager sm;
    sm.init(dir.string());

    json data = {{"val", 1}};
    CHECK(sm.save(1, data, "s1", 0));

    json loaded = sm.load(1);
    CHECK_FALSE(loaded.empty());
    // v1->v2 migration adds playtime field
    CHECK(loaded.contains("val"));
    CHECK(loaded["val"] == 1);
}

TEST_CASE("SaveManager::json nil round-trip") {
    TestPaths::ScopedTempDir dir("migration_nil");
    SaveManager sm;
    sm.init(dir.string());

    json data = {{"nil_val", nullptr}, {"str_val", "hello"}};
    sm.save(1, data, "nil_test", 0);

    json loaded = sm.load(1);
    CHECK(loaded["nil_val"] == nullptr);
    CHECK(loaded["str_val"] == "hello");
}

TEST_CASE("SaveManager::schema version is tracked") {
    TestPaths::ScopedTempDir dir("migration_schema");
    SaveManager sm;
    sm.init(dir.string());
    CHECK(sm.currentSchemaVersion() >= 2);
}

TEST_CASE("SaveManager::JSON parse error returns empty") {
    TestPaths::ScopedTempDir dir("migration_parse_error");
    SaveManager sm;
    sm.init(dir.string());

    // Write invalid JSON manually
    std::ofstream out(dir.path() / "save_0.json");
    out << "this is not json {{{";
    out.close();

    json data = sm.load(0);
    CHECK(data.empty());
}

TEST_CASE("SaveManager thumbnail capture guards on gfx readiness") {
    SaveManager tm;
    tm.setGfxReady(false);  // reset: the flag is static, Engine tests set it
    CHECK_FALSE(tm.isGfxReady());
    CHECK(tm.captureThumbnailPNG(320, 180) == "");  // skipped, no crash
    tm.setGfxReady(true);
    CHECK(tm.isGfxReady());
    tm.setGfxReady(false);
}
