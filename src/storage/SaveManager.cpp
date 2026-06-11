// ===========================================================================
//  Caesura (AmeKAG) -- SaveManager.cpp
//  JSON save/load with schema versioning and migration chain.
//  Uses nlohmann/json v3.11.3 for robust structured serialization.
//  Save format: {"schema_version":1,"timestamp":12345,"scene":"...",
//                "token_index":5,"thumbnail":"...","engine_version":"1.0.0",
//                "data":{...}}
//  Encrypted format: "CAES" [12-byte nonce] [16-byte tag] [ciphertext...]
// ===========================================================================

#include "SaveManager.h"
#include "ISaveProvider.h"
#include "../archive/CryptoEngine.h"
#include <bgfx/bgfx.h>
#include <vector>

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
//  Pluggable save provider (SU-6)
// ============================================================================

void SaveManager::setSaveProvider(std::unique_ptr<ISaveProvider> provider) {
    m_saveProvider = std::move(provider);
    printf("[SaveManager] Custom save provider installed.\n");
}

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
//  Encryption (SU-2) -- AES-256-GCM via CryptoEngine
//  Encrypted save format: [4-byte "CAES"][12-byte nonce][16-byte tag][ciphertext]
// ============================================================================

void SaveManager::setEncryptionKey(const uint8_t key[32]) {
    std::memcpy(m_encryptKey, key, 32);
    m_keySet = true;
    printf("[SaveManager] Encryption key set (AES-256-GCM)\n");
}

void SaveManager::clearEncryptionKey() {
    m_keySet = false;
    std::memset(m_encryptKey, 0, sizeof(m_encryptKey));
    printf("[SaveManager] Encryption key cleared\n");
}

// ============================================================================
//  File I/O (with optional AES-256-GCM encryption)
// ============================================================================

std::string SaveManager::slotPath(int slot) const {
    return m_saveDir + "save_" + std::to_string(slot) + ".json";
}

static constexpr size_t MAX_SAVE_SIZE = 10 * 1024 * 1024;  // 10 MiB

std::string SaveManager::readFile(const std::string& path) {
    if (m_saveProvider) return m_saveProvider->readFile(path);
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";

    in.seekg(0, std::ios::end);
    auto sz = in.tellg();
    if (sz <= 0 || static_cast<size_t>(sz) > MAX_SAVE_SIZE) return "";

    std::string content(static_cast<size_t>(sz), '\0');
    in.seekg(0, std::ios::beg);
    in.read(&content[0], sz);

    // Decrypt if key is set and data starts with "CAES" magic
    if (m_keySet && content.size() >= 4 && std::memcmp(content.data(), "CAES", 4) == 0) {
        if (content.size() < 32) {
            fprintf(stderr, "[SaveManager] Encrypted save too short\n");
            return "";
        }
        const auto* nonce = reinterpret_cast<const uint8_t*>(content.data() + 4);
        const auto* tag   = reinterpret_cast<const uint8_t*>(content.data() + 16);
        const auto* ct    = reinterpret_cast<const uint8_t*>(content.data() + 32);
        size_t ctLen = content.size() - 32;

        auto plain = carc::CryptoEngine::decrypt(ct, ctLen, m_encryptKey, nonce, tag);
        if (plain.empty()) {
            fprintf(stderr, "[SaveManager] Decryption failed (wrong key or corrupted data)\n");
            return "";
        }
        return std::string(reinterpret_cast<char*>(plain.data()), plain.size());
    }

    return content;
}

bool SaveManager::writeFile(const std::string& path, const std::string& content) {
    if (m_saveProvider) return m_saveProvider->writeFile(path, content);

    std::string dataToWrite;
    if (m_keySet) {
        uint8_t nonce[12];
        uint8_t tag[16];
        carc::CryptoEngine::generateNonce(nonce);

        auto cipher = carc::CryptoEngine::encrypt(
            reinterpret_cast<const uint8_t*>(content.data()), content.size(),
            m_encryptKey, nonce, tag);
        if (cipher.empty()) {
            fprintf(stderr, "[SaveManager] Encryption failed\n");
            return false;
        }

        dataToWrite.reserve(4 + 12 + 16 + cipher.size());
        dataToWrite.append("CAES", 4);
        dataToWrite.append(reinterpret_cast<char*>(nonce), 12);
        dataToWrite.append(reinterpret_cast<char*>(tag), 16);
        dataToWrite.append(reinterpret_cast<char*>(cipher.data()), cipher.size());
    } else {
        dataToWrite = content;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        fprintf(stderr, "[SaveManager] Failed to open file for writing: %s\n", path.c_str());
        return false;
    }
    out.write(dataToWrite.c_str(), static_cast<std::streamsize>(dataToWrite.size()));
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
        fprintf(stderr, "[SaveManager] JSON parse error: %s\n", e.what());
        return json();
    }

    uint64_t ts       = envelope.value("timestamp", uint64_t(0));
    std::string scene = envelope.value("scene", "");
    int tokenIdx      = envelope.value("token_index", 0);
    std::string thumb = envelope.value("thumbnail", "");
    int schemaVer     = envelope.value("schema_version", 1);
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

    int consecutiveEmpty = 0;
    for (int slot = 0; slot < 1000; slot++) {
        std::string contents = readFile(slotPath(slot));
        if (contents.empty()) {
            consecutiveEmpty++;
            if (consecutiveEmpty >= 8) break;
            continue;
        }
        consecutiveEmpty = 0;

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
    if (m_saveProvider) return m_saveProvider->deleteFile(path);
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
    // v1 -> v2: Add playtime field
    registerMigration(1, 2, [](const json& data) -> json {
        json result = data;
        if (!result.contains("playtime")) result["playtime"] = 0;
        return result;
    });
    // v2 -> v3: Add MiniGame 3D state
    registerMigration(2, 3, [](const json& data) -> json {
        json result = data;
        if (!result.contains("minigame")) result["minigame"] = json::object();
        return result;
    });
    // v3 -> v4: Add Live2D state
    registerMigration(3, 4, [](const json& data) -> json {
        json result = data;
        if (!result.contains("live2d")) result["live2d"] = json::object();
        return result;
    });
    // v4 -> v5: Add Editor state
    registerMigration(4, 5, [](const json& data) -> json {
        json result = data;
        if (!result.contains("editor")) result["editor"] = json::object();
        return result;
    });
}


// ============================================================================
//  Thumbnail capture (SU-4 stub)
// ============================================================================
std::string SaveManager::captureThumbnailPNG(int width, int height) {
    (void)width; (void)height;
    char path[256];
    static int thumbCounter = 0;
    snprintf(path, sizeof(path), "save_thumb_%d.png", thumbCounter++);
    if (thumbCounter > 99) thumbCounter = 0;
    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path);
    bgfx::frame();
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return "";
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();
    std::remove(path);
    static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((size + 2) / 3) * 4);
    for (size_t j = 0; j < size; j += 3) {
        unsigned char a = buffer[j];
        unsigned char b = (j + 1 < size) ? buffer[j + 1] : 0;
        unsigned char c = (j + 2 < size) ? buffer[j + 2] : 0;
        result += b64[a >> 2];
        result += b64[((a & 3) << 4) | (b >> 4)];
        result += (j + 1 < size) ? b64[((b & 15) << 2) | (c >> 6)] : '=';
        result += (j + 2 < size) ? b64[c & 63] : '=';
    }
    return result;
}

} // namespace Caesura
