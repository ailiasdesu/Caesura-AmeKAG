// ===========================================================================
//  Caesura (AmeKAG) -- SaveManager.h
//  Spec [6.1]: JSON save/load system with schema versioning.
//  SaveMeta: slot, timestamp, sceneName, thumbnail, tokenIndex, schemaVersion
//  SaveManager (singleton): init(), save(), load(), listSaves(), migrate()
//  Uses raw JSON string building -- no external JSON library dependency.
//  Migration: version chain walk (1¡ú2, 2¡ú3, etc.) with migration scripts.
// ===========================================================================

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <unordered_map>

struct lua_State;

namespace Caesura {

// -- SaveMeta --------------------------------------------------------------
struct SaveMeta {
    int         slot         = 0;
    uint64_t    timestamp    = 0;
    std::string sceneName;
    std::string thumbnail;     // base64-encoded PNG thumbnail
    int         tokenIndex    = 1;
    int         schemaVersion = 1;
};

// Migration function signature: takes JSON string, returns migrated JSON string
using MigrationFn = std::function<std::string(const std::string&)>;

// -- SaveManager -----------------------------------------------------------
class SaveManager {
public:
    static SaveManager& instance();

    // Engine version for save compatibility (Spec U6: archive version management)
    static const char* ENGINE_VERSION;

    SaveManager(const SaveManager&) = delete;
    SaveManager& operator=(const SaveManager&) = delete;

    // -- Lifecycle ----------------------------------------------------------
    // init(saveDir): set save directory and register built-in migrations
    void init(const std::string& saveDir);

    // -- Save/Load ----------------------------------------------------------
    // Save a slot: wraps jsonData with metadata (schema_version, timestamp, etc.)
    // thumbnailPng is base64-encoded PNG (empty string = no thumbnail)
    bool save(int slot, const std::string& jsonData,
              const std::string& sceneName,
              int tokenIndex,
              const std::string& thumbnailPng = "");

    // Load a slot: reads JSON file, runs migration if needed
    // Returns raw data string from the "data" field, or empty on failure
    std::string load(int slot, SaveMeta* outMeta = nullptr);

    // -- Listing ------------------------------------------------------------
    // List all save slots found on disk (sorted by slot number)
    std::vector<SaveMeta> listSaves();

    // Check if a slot exists
    bool slotExists(int slot);

    // Delete a save slot
    bool deleteSlot(int slot);

    // -- Migration ----------------------------------------------------------
    // Register a migration from fromVersion ¡ú toVersion
    void registerMigration(int fromVersion, int toVersion, MigrationFn fn);

    // Engine-level version check and migration (Spec U6)
    bool migrateSave(std::string& jsonData);

    // Migrate a JSON string through version chain (fromVersion ¡ú latest)
    std::string migrate(const std::string& jsonData, int fromVersion);

    // Current schema version (latest)
    int currentSchemaVersion() const { return m_currentSchemaVersion; }

    // -- JSON helpers (static, reusable) ------------------------------------
    // Build a simple JSON object string from key-value string pairs
    static std::string jsonEscape(const std::string& s);
    static std::string jsonBuildObj(
        const std::vector<std::pair<std::string, std::string>>& fields);

    // Extract value for a top-level string key from simple JSON
    static std::string jsonGetString(const std::string& json, const std::string& key);
    static int         jsonGetInt(const std::string& json, const std::string& key);
    static uint64_t    jsonGetUint64(const std::string& json, const std::string& key);
    static std::string jsonGetRawValue(const std::string& json, const std::string& key);

private:
    SaveManager() = default;

    std::string m_saveDir;
    int m_currentSchemaVersion = 1;

    // Migration chain: maps from-version ¡ú {to-version, migration function}
    // e.g., {1, {2, fn1_2}}, {2, {3, fn2_3}}
    std::unordered_map<int, std::pair<int, MigrationFn>> m_migrations;

    // Internal helpers
    std::string slotPath(int slot) const;
    std::string readFile(const std::string& path);
    bool writeFile(const std::string& path, const std::string& content);

    void registerBuiltinMigrations();
};

} // namespace Caesura
