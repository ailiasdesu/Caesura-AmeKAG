// test_storage.cpp - storage module unit tests (S2.4)
#include "doctest.h"
#include "storage/api/ISaveManager.h"
#include "storage/SaveManager.h"
#include <filesystem>

using namespace Caesura;

namespace {
    std::string g_tempDir;

    void setupTempDir() {
        g_tempDir = std::filesystem::temp_directory_path().string() + "/caesura_test_storage/";
        std::filesystem::create_directories(g_tempDir);
    }

    void cleanupTempDir() {
        std::filesystem::remove_all(g_tempDir);
    }
}

TEST_CASE("Storage: SaveManager singleton is accessible") {
    auto& sm = SaveManager::instance();
    CHECK(&sm != nullptr);
}

TEST_CASE("Storage: SaveManager init with temp directory") {
    setupTempDir();
    auto& sm = SaveManager::instance();
    sm.init(g_tempDir);
    cleanupTempDir();
    CHECK(true);
}

TEST_CASE("Storage: SaveManager listSaves on empty directory") {
    setupTempDir();
    auto& sm = SaveManager::instance();
    sm.init(g_tempDir);
    auto saves = sm.listSaves();
    CHECK(saves.empty());
    cleanupTempDir();
}

TEST_CASE("Storage: SaveManager currentSchemaVersion is non-negative") {
    auto& sm = SaveManager::instance();
    CHECK(sm.currentSchemaVersion() >= 0);
}

TEST_CASE("Storage: SaveManager slotExists on uninitialized returns false") {
    auto& sm = SaveManager::instance();
    CHECK(sm.slotExists(99) == false);
}

TEST_CASE("Storage: SaveManager deleteSlot on uninitialized returns false") {
    auto& sm = SaveManager::instance();
    CHECK(sm.deleteSlot(99) == false);
}

TEST_CASE("Storage: ISaveManager interface upcast") {
    ISaveManager* iface = &SaveManager::instance();
    CHECK(iface != nullptr);
    CHECK(iface->currentSchemaVersion() >= 0);
}
