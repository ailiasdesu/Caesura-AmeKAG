// ===========================================================================
//  Caesura (AmeKAG) -- SaveManager.cpp
//  JSON save/load with schema versioning and migration chain.
//  Uses raw string building for JSON -- no external JSON library dependency.
//  Save format: {"schema_version":1,"timestamp":12345,"scene":"...","token_index":5,"thumbnail":"...","data":{...}}
// ===========================================================================

#include "SaveManager.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>
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

    // Ensure trailing separator
    if (!m_saveDir.empty() && m_saveDir.back() != '/' && m_saveDir.back() != '\\') {
        m_saveDir += '/';
    }

    // Create save directory if it doesn't exist
    mkdir_impl(m_saveDir.c_str());

    registerBuiltinMigrations();
    printf("[SaveManager] Initialized. Save dir: %s (schema v%d)\n",
           m_saveDir.c_str(), m_currentSchemaVersion);
}

// ============================================================================
//  JSON helpers (static)
// ============================================================================

std::string SaveManager::jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string SaveManager::jsonBuildObj(
    const std::vector<std::pair<std::string, std::string>>& fields)
{
    std::string out = "{";
    bool first = true;
    for (const auto& kv : fields) {
        if (!first) out += ",";
        first = false;
        // key is always a quoted string
        out += "\"" + jsonEscape(kv.first) + "\":" + kv.second;
    }
    out += "}";
    return out;
}

// Simple JSON value extractors.
// Flat key-value extraction + raw nested value extraction for "data" field.
// Adequate for current save format where only "data" may be a nested object.

std::string SaveManager::jsonGetString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    size_t end = pos;
    while (end < json.size()) {
        if (json[end] == '"' && (end == 0 || json[end - 1] != '\\')) break;
        end++;
    }
    return json.substr(pos, end - pos);
}

int SaveManager::jsonGetInt(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return 0;
    pos += search.size();
    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    size_t end = pos;
    while (end < json.size() && (json[end] >= '0' && json[end] <= '9' || json[end] == '-')) end++;
    if (end == pos) return 0;
    return std::stoi(json.substr(pos, end - pos));
}

uint64_t SaveManager::jsonGetUint64(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return 0;
    pos += search.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    size_t end = pos;
    while (end < json.size() && json[end] >= '0' && json[end] <= '9') end++;
    if (end == pos) return 0;
    return std::stoull(json.substr(pos, end - pos));
}

// Extract the raw JSON value for a key (string, number, object, or array).
// Handles brace/bracket depth for nested values and escaped quotes in strings.
std::string SaveManager::jsonGetRawValue(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();

    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return "";

    char first = json[pos];
    if (first == '"') {
        // String value ¡ª find matching closing quote (respecting escapes)
        size_t start = pos + 1;
        size_t end = start;
        while (end < json.size()) {
            if (json[end] == '\\') { end += 2; continue; }
            if (json[end] == '"') break;
            end++;
        }
        return json.substr(start, end - start);
    }
    if (first == '{' || first == '[') {
        // Object or array ¡ª track depth with matching close char
        char close = (first == '{') ? '}' : ']';
        int depth = 0;
        size_t start = pos;
        bool inString = false;
        for (size_t i = pos; i < json.size(); i++) {
            char ch = json[i];
            if (inString) {
                if (ch == '\\') { i++; continue; }
                if (ch == '"') inString = false;
                continue;
            }
            if (ch == '"') { inString = true; continue; }
            if (ch == first) depth++;
            if (ch == close) {
                if (depth == 0) return json.substr(start, i - start + 1);
                depth--;
            }
        }
    }
    // Number/bool/null ¡ª read until comma/whitespace/closing brace
    size_t start = pos;
    size_t end = pos;
    while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != ']') end++;
    // Trim trailing whitespace
    while (end > start && (json[end-1] == ' ' || json[end-1] == '\t' || json[end-1] == '\n')) end--;
    return json.substr(start, end - start);
}

// ============================================================================
//  Internal helpers
// ============================================================================

std::string SaveManager::slotPath(int slot) const {
    return m_saveDir + "save_" + std::to_string(slot) + ".json";
}

std::string SaveManager::readFile(const std::string& path) {
    FILE* f = nullptr;
#ifdef _WIN32
    fopen_s(&f, path.c_str(), "rb");
#else
    f = fopen(path.c_str(), "rb");
#endif
    if (!f) return "";

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return "";
    }

    std::string content(size, '\0');
    size_t read = fread(&content[0], 1, size, f);
    fclose(f);

    content.resize(read);
    return content;
}

bool SaveManager::writeFile(const std::string& path, const std::string& content) {
    FILE* f = nullptr;
#ifdef _WIN32
    fopen_s(&f, path.c_str(), "wb");
#else
    f = fopen(path.c_str(), "wb");
#endif
    if (!f) {
        fprintf(stderr, "[SaveManager] Cannot open file for writing: %s\n", path.c_str());
        return false;
    }

    size_t written = fwrite(content.data(), 1, content.size(), f);
    fclose(f);

    return written == content.size();
}

// ============================================================================
//  Save
// ============================================================================

bool SaveManager::save(int slot, const std::string& jsonData,
                       const std::string& sceneName,
                       int tokenIndex,
                       const std::string& thumbnailPng)
{
    uint64_t ts = static_cast<uint64_t>(std::time(nullptr));

    std::string escapedData = jsonEscape(jsonData);

    // Minimal JSON building -- the data field is embedded as a string
    // (pre-escaped by the caller or Lua side via jsonEscape)
    std::string json = jsonBuildObj({
        {"schema_version", std::to_string(m_currentSchemaVersion)},
        {"engine_version", "\"" + std::string(ENGINE_VERSION) + "\""},
        {"timestamp",      std::to_string(ts)},
        {"scene",          "\"" + jsonEscape(sceneName) + "\""},
        {"token_index",    std::to_string(tokenIndex)},
        {"thumbnail",      "\"" + jsonEscape(thumbnailPng) + "\""},
        {"data",           jsonData},  // embedded JSON object (caller provides valid JSON)
    });

    std::string path = slotPath(slot);
    bool ok = writeFile(path, json);

    if (ok) {
        printf("[SaveManager] Saved slot %d (%s, token=%d) â†?%s\n",
               slot, sceneName.c_str(), tokenIndex, path.c_str());
    } else {
        fprintf(stderr, "[SaveManager] Failed to save slot %d\n", slot);
    }

    return ok;
}

// ============================================================================
//  Load
// ============================================================================

std::string SaveManager::load(int slot, SaveMeta* outMeta) {
    std::string path = slotPath(slot);
    std::string json = readFile(path);

    if (json.empty()) {
        fprintf(stderr, "[SaveManager] No save data for slot %d (%s)\n", slot, path.c_str());
        return "";
    }

    int ver = jsonGetInt(json, "schema_version");
    if (ver < 1) {
        fprintf(stderr, "[SaveManager] Invalid schema version in slot %d\n", slot);
        return "";
    }

    // Migrate if schema version is outdated
    if (ver < m_currentSchemaVersion) {
        printf("[SaveManager] Migrating slot %d: schema v%d â†?v%d\n",
               slot, ver, m_currentSchemaVersion);
        json = migrate(json, ver);

        // Re-write the migrated save back to disk (transparent upgrade)
        writeFile(path, json);
    }

    // Populate metadata

    // Engine version migration (Spec U6)
    migrateSave(json);
    if (outMeta) {
        outMeta->slot          = slot;
        outMeta->timestamp     = jsonGetUint64(json, "timestamp");
        outMeta->sceneName     = jsonGetString(json, "scene");
        outMeta->thumbnail     = jsonGetString(json, "thumbnail");
        outMeta->tokenIndex    = jsonGetInt(json, "token_index");
        outMeta->schemaVersion = m_currentSchemaVersion;
    }

    // Extract the "data" field using reusable JSON value extractor
    std::string rawData = jsonGetRawValue(json, "data");
    if (rawData.empty()) {
        fprintf(stderr, "[SaveManager] Slot %d: missing or empty 'data' field\n", slot);
        return "";
    }

    printf("[SaveManager] Loaded slot %d (%s, schema v%d)\n",
           slot, outMeta ? outMeta->sceneName.c_str() : "?", ver);

    return rawData;
}

// ============================================================================
//  List
// ============================================================================

std::vector<SaveMeta> SaveManager::listSaves() {
    std::vector<SaveMeta> result;

    // Scan save directory for save_N.json files
    for (int slot = 0; slot < 1000; slot++) {
        std::string path = slotPath(slot);
        std::string json = readFile(path);
        if (json.empty()) continue;

        SaveMeta meta;
        meta.slot          = slot;
        meta.timestamp     = jsonGetUint64(json, "timestamp");
        meta.sceneName     = jsonGetString(json, "scene");
        meta.thumbnail     = jsonGetString(json, "thumbnail");
        meta.tokenIndex    = jsonGetInt(json, "token_index");
        meta.schemaVersion = jsonGetInt(json, "schema_version");

        result.push_back(meta);
    }

    // Sort by slot (already in order from linear scan, but be safe)
    std::sort(result.begin(), result.end(),
              [](const SaveMeta& a, const SaveMeta& b) { return a.slot < b.slot; });

    printf("[SaveManager] Found %zu save(s)\n", result.size());
    return result;
}

bool SaveManager::slotExists(int slot) {
    std::string path = slotPath(slot);
    std::string json = readFile(path);
    return !json.empty();
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
//  Migration
// ============================================================================

void SaveManager::registerMigration(int fromVersion, int toVersion, MigrationFn fn) {
    m_migrations[fromVersion] = {toVersion, fn};
    if (toVersion > m_currentSchemaVersion) {
        m_currentSchemaVersion = toVersion;
    }
    printf("[SaveManager] Registered migration: v%d â†?v%d\n", fromVersion, toVersion);
}

std::string SaveManager::migrate(const std::string& jsonData, int fromVersion) {
    std::string current = jsonData;
    int ver = fromVersion;

    // Walk the migration chain
    while (true) {
        auto it = m_migrations.find(ver);
        if (it == m_migrations.end()) break;  // no more migrations

        int nextVer = it->second.first;
        const MigrationFn& fn = it->second.second;

        printf("[SaveManager] Applying migration v%d â†?v%d\n", ver, nextVer);
        current = fn(current);
        ver = nextVer;
    }

    // Update schema_version field in the migrated JSON
    // Replace "schema_version":<old> with "schema_version":<new>
    std::string oldVerStr = "\"schema_version\":" + std::to_string(fromVersion);
    std::string newVerStr = "\"schema_version\":" + std::to_string(ver);

    size_t pos = current.find(oldVerStr);
    if (pos != std::string::npos) {
        current.replace(pos, oldVerStr.size(), newVerStr);
    }

    return current;
}

// ============================================================================
//  Built-in migration scripts
// ============================================================================

// ============================================================================
//  Engine version migration (Spec U6: archive version management)
// ============================================================================

bool SaveManager::migrateSave(std::string& jsonData) {
    std::string ver = jsonGetString(jsonData, "engine_version");
    if (ver.empty()) ver = "0.0.0";

    if (ver == ENGINE_VERSION) return true;  // Current version, no migration needed

    printf("[SaveManager] Engine version mismatch: %s (engine %s) -- warn but continue loading\n",
           ver.c_str(), ENGINE_VERSION);

    // Future: chain migration functions here
    // if (ver == "1.0.0") { migrate_v1_to_v2(jsonData); ver = "1.1.0"; }

    // Update the engine_version field in the JSON
    std::string oldField = "\"engine_version\":\"" + ver + "\"";
    std::string newField = "\"engine_version\":\"" + std::string(ENGINE_VERSION) + "\"";
    size_t pos = jsonData.find(oldField);
    if (pos != std::string::npos) {
        jsonData.replace(pos, oldField.size(), newField);
    }

    return true;
}

void SaveManager::registerBuiltinMigrations() {
    // v1 -> v2: Add "playtime" field to track total play time across saves.
    registerMigration(1, 2, [](const std::string& json) -> std::string {
        // Idempotent: skip if playtime already present
        if (json.find("\"playtime\":") != std::string::npos) return json;

        std::string result = json;
        std::string marker = "\"data\":{";
        size_t pos = result.find(marker);
        if (pos != std::string::npos) {
            size_t insertPos = pos + marker.size();
            // Avoid trailing comma for empty data object "data":{}
            char nextChar = (insertPos < result.size()) ? result[insertPos] : 0;
            if (nextChar == '}') {
                result.insert(insertPos, "\"playtime\":0");
            } else {
                result.insert(insertPos, "\"playtime\":0,");
            }
        }
        return result;
    });
}

} // namespace Caesura
