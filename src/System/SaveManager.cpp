// ===========================================================================
//  Caesura (AmeKAG) -- SaveManager.cpp
//  JSON save/load with schema versioning and migration chain.
//  Uses nlohmann/json v3.11.3 for robust structured serialization.
//  Save format: {"schema_version":1,"timestamp":12345,"scene":"...",
//                "token_index":5,"thumbnail":"...","engine_version":"1.0.0",
//                "data":{...}}
// ===========================================================================

#include "SaveManager.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <algorithm>

#ifdef _WIN32
#include <direct.h>
#define mkdir_impl(p) _mkdir(p)
#else
#include <sys/stat.h>
#define mkdir_impl(p) mkdir(p, 0755)
#endif

namespace Caesura {

// Engine version string (Spec U6: archive version management)
const char* SaveManager::ENGINE_VERSION = "1.0.0";

// ============================================================================
//  Singleton
// ============================================================================

SaveManager& SaveManager::instance() {
    static SaveManager inst;
    return inst;
}

// ============================================================================
//  Lifecycle
// ============================================================================

void SaveManager::init(const std::string& saveDir) {
    m_saveDir = saveDir;

    if (!m_saveDir.empty() && m_saveDir.back() != '/' && m_saveDir.back() != '\\') {
        m_saveDir += '/';
    }

    mkdir_impl(m_saveDir.c_str());

    registerBuiltinMigrations();
    printf("[SaveManager] Initialized. Save dir: %s (schema v%d)\n",
           m_saveDir.c_str(), m_currentSchemaVersion);
}

// ============================================================================
//  File I/O
// ============================================================================

std::string SaveManager::slotPath(int slot) const {
    return m_saveDir + "save_" + std::to_string(slot) + ".json";
}

static constexpr size_t MAX_SAVE_SIZE = 10 * 1024 * 1024;  // 10 MiB

std::string SaveManager::readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";

    in.seekg(0, std::ios::end);
    auto sz = in.tellg();
    if (sz <= 0 || static_cast<size_t>(sz) > MAX_SAVE_SIZE) return "";

    std::string content(static_cast<size_t>(sz), '\0');
    in.seekg(0, std::ios::beg);
    in.read(&content[0], sz);
    return content;
}

bool SaveManager::writeFile(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        fprintf(stderr, "[SaveManager] Failed to open file for writing: %s\n", path.c_str());
        return false;
    }
    out.write(content.c_str(), static_cast<std::streamsize>(content.size()));
    return out.good();
}

// ============================================================================
//  Save (write structured JSON to disk)
// ============================================================================

bool SaveManager::save(int slot, const json& gameData,
                       const std::string& sceneName,
                       int tokenIndex,
                       const std::string& thumbnailPng) {
    if (m_saveDir.empty()) {
        fprintf(stderr, "[SaveManager] Not initialized; call init() first.\n");
        return false;
    }

    json envelope;
    envelope["schema_version"] = m_currentSchemaVersion;
    envelope["timestamp"]      = static_cast<uint64_t>(time(nullptr));
    envelope["scene"]          = sceneName;
    envelope["token_index"]    = tokenIndex;
    envelope["thumbnail"]      = thumbnailPng;
    envelope["engine_version"] = ENGINE_VERSION;
    envelope["data"]           = gameData;

    std::string path    = slotPath(slot);
    std::string jsonStr = envelope.dump(2);  // 2-space indent

    bool ok = writeFile(path, jsonStr);
    if (ok) {
        printf("[SaveManager] Saved slot %d (%s, token %d)\n",
               slot, sceneName.c_str(), tokenIndex);
    }
    return ok;
}

// ============================================================================
//  Load (read JSON from disk, return structured data)
// ============================================================================

json SaveManager::load(int slot, SaveMeta* outMeta) {
    std::string contents = readFile(slotPath(slot));
    if (contents.empty()) return json();

    json envelope;
    try {
        envelope = json::parse(contents);
    } catch (const json::exception& e) {
        fprintf(stderr, "[SaveManager] JSON parse error in slot %d: %s\n", slot, e.what());
        return json();
    }

    int schemaVer         = envelope.value("schema_version", 1);
    uint64_t ts           = envelope.value("timestamp", uint64_t(0));
    std::string scene     = envelope.value("scene", "");
    int tokenIdx          = envelope.value("token_index", 0);
    std::string thumb     = envelope.value("thumbnail", "");
    std::string engineVer = envelope.value("engine_version", "");

    if (outMeta) {
        outMeta->slot          = slot;
        outMeta->timestamp     = ts;
        outMeta->sceneName     = scene;
        outMeta->tokenIndex    = tokenIdx;
        outMeta->thumbnail     = thumb;
        outMeta->schemaVersion = schemaVer;
    }

    // Handle schema migration on the "data" sub-object
    json data = envelope.value("data", json());
    if (schemaVer < m_currentSchemaVersion) {
        data = migrate(data, schemaVer);
        if (outMeta) outMeta->schemaVersion = m_currentSchemaVersion;
    }

    if (!engineVer.empty() && engineVer != ENGINE_VERSION) {
        printf("[SaveManager] Engine version mismatch: %s (engine %s) -- continue loading\n",
               engineVer.c_str(), ENGINE_VERSION);
    }

    printf("[SaveManager] Loaded slot %d (v%d, %s, token %d)\n",
           slot, schemaVer, scene.c_str(), tokenIdx);
    return data;
}

// ============================================================================
//  List / delete
// ============================================================================

std::vector<SaveMeta> SaveManager::listSaves() {
    std::vector<SaveMeta> result;
    if (m_saveDir.empty()) return result;

    for (int slot = 0; slot < 1000; slot++) {
        std::string contents = readFile(slotPath(slot));
        if (contents.empty()) continue;

        json envelope;
        try {
            envelope = json::parse(contents);
        } catch (const json::exception&) {
            continue;
        }

        SaveMeta meta;
        meta.slot          = slot;
        meta.timestamp     = envelope.value("timestamp", uint64_t(0));
        meta.sceneName     = envelope.value("scene", "");
        meta.thumbnail     = envelope.value("thumbnail", "");
        meta.tokenIndex    = envelope.value("token_index", 0);
        meta.schemaVersion = envelope.value("schema_version", 1);

        result.push_back(meta);
    }

    std::sort(result.begin(), result.end(),
              [](const SaveMeta& a, const SaveMeta& b) { return a.slot < b.slot; });

    printf("[SaveManager] Found %zu save(s)\n", result.size());
    return result;
}

bool SaveManager::slotExists(int slot) {
    return !readFile(slotPath(slot)).empty();
}

bool SaveManager::deleteSlot(int slot) {
    std::string path = slotPath(slot);
    if (remove(path.c_str()) == 0) {
        printf("[SaveManager] Deleted slot %d\n", slot);
        return true;
    }
    fprintf(stderr, "[SaveManager] Failed to delete slot %d\n", slot);
    return false;
}

// ============================================================================
//  Migration (with structured JSON)
// ============================================================================

void SaveManager::registerMigration(int fromVersion, int toVersion, MigrationFn fn) {
    m_migrations[fromVersion] = {toVersion, fn};
    if (toVersion > m_currentSchemaVersion) {
        m_currentSchemaVersion = toVersion;
    }
    printf("[SaveManager] Registered migration: v%d -> v%d\n", fromVersion, toVersion);
}

json SaveManager::migrate(const json& data, int fromVersion) {
    json current = data;
    int ver = fromVersion;

    while (true) {
        auto it = m_migrations.find(ver);
        if (it == m_migrations.end()) break;

        int nextVer = it->second.first;
        printf("[SaveManager] Applying migration v%d -> v%d\n", ver, nextVer);
        current = it->second.second(current);
        ver = nextVer;
    }

    return current;
}

// ============================================================================
//  Built-in migration scripts
// ============================================================================

void SaveManager::registerBuiltinMigrations() {
    // v1 -> v2: Add "playtime" field to track total play time across saves.
    registerMigration(1, 2, [](const json& data) -> json {
        json result = data;
        if (!result.contains("playtime")) {
            result["playtime"] = 0;
        }
        return result;
    });
}

} // namespace Caesura
