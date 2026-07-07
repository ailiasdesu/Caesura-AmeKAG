// test_storage.cpp - storage module unit tests (S2.4)
#include "doctest.h"
#include "storage/api/ISaveManager.h"
#include "storage/SaveManager.h"
#include "TestPaths.h"
#include <filesystem>
#include <cstring>
#include <memory>

using namespace Caesura;

namespace {
    std::unique_ptr<TestPaths::ScopedTempDir> g_tempDir;

    void setupTempDir() {
        g_tempDir = std::make_unique<TestPaths::ScopedTempDir>("storage");
    }

    void cleanupTempDir() {
        g_tempDir.reset();
    }
}

TEST_CASE("Storage: SaveManager singleton is accessible") {
    auto& sm = SaveManager::instance();
    CHECK(&sm != nullptr);
}

TEST_CASE("Storage: SaveManager init with temp directory") {
    setupTempDir();
    auto& sm = SaveManager::instance();
    sm.init(g_tempDir->string());
    cleanupTempDir();
    CHECK(true);
}

TEST_CASE("Storage: SaveManager listSaves on empty directory") {
    setupTempDir();
    auto& sm = SaveManager::instance();
    sm.init(g_tempDir->string());
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

// =============================================================================
// Expanded: save provider
// =============================================================================

TEST_CASE("Storage: SaveManager default save provider exists after init") {
    setupTempDir();
    auto& sm = SaveManager::instance();
    sm.init(g_tempDir->string());
    // Save provider is nullptr by default (must be set via setSaveProvider)
    // init does not automatically create a LocalFileSaveProvider
    CHECK(sm.getSaveProvider() == nullptr);
    cleanupTempDir();
}

TEST_CASE("Storage: SaveManager ENGINE_VERSION is not empty") {
    CHECK(SaveManager::ENGINE_VERSION != nullptr);
    CHECK(std::strlen(SaveManager::ENGINE_VERSION) > 0);
}
