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
#include "api/ISaveProvider.h"
#include "../di/BackendRegistry.h"
#include "../archive/api/ICryptoEngine.h"
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

namespace {

void secureErase(void* memory, size_t size) noexcept {
    auto* bytes = static_cast<volatile uint8_t*>(memory);
    while (size-- > 0) {
        *bytes++ = 0;
    }
}

} // namespace

// Engine version string (Spec U6: archive version management)
const char* SaveManager::ENGINE_VERSION = "1.0.0";

SaveManager::SaveManager() = default;
SaveManager::~SaveManager() {
    secureErase(m_encryptKey, sizeof(m_encryptKey));
    m_keySet = false;
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
    secureErase(m_encryptKey, sizeof(m_encryptKey));
    m_keySet = false;
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

        auto* crypto = BackendRegistry::instance().getCryptoEngine();
        if (!crypto) {
            fprintf(stderr, "[SaveManager] CryptoEngine not initialized\n");
            return "";
        }
        auto plain = crypto->decrypt(ct, ctLen, m_encryptKey, 32, nonce, 12, tag, 16);
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
        auto* crypto = BackendRegistry::instance().getCryptoEngine();
        if (!crypto) {
            fprintf(stderr, "[SaveManager] CryptoEngine not initialized\n");
            return false;
        }
        crypto->generateNonce(nonce, 12);

        auto cipher = crypto->encrypt(            reinterpret_cast<const uint8_t*>(content.data()), content.size(),
            m_encryptKey, 32, nonce, 12, tag, 16);
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

    // Atomic write: write to a temp file next to the target, flush, then
    // rename. A crash mid-write leaves the previous save intact instead of
    // truncating it (rename is atomic on the same filesystem).
    const std::string tmpPath = path + ".tmp";
    std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        fprintf(stderr, "[SaveManager] Failed to open file for writing: %s\n", tmpPath.c_str());
        return false;
    }
    out.write(dataToWrite.c_str(), static_cast<std::streamsize>(dataToWrite.size()));
    out.flush();
    if (!out.good()) {
        fprintf(stderr, "[SaveManager] Write failed for %s\n", tmpPath.c_str());
        std::filesystem::remove(tmpPath);  // no stale partial temp file
        return false;
    }
    out.close();
    std::error_code ec;
    std::filesystem::rename(tmpPath, path, ec);
    if (ec) {
        fprintf(stderr, "[SaveManager] Rename failed: %s\n", ec.message().c_str());
        std::filesystem::remove(tmpPath);
        return false;
    }
    return true;
}

// ============================================================================
//  Save (write structured JSON to disk)
// ============================================================================

bool SaveManager::save(int slot, const json& gameData,
                       const std::string& sceneName,
                       int tokenIndex,
                       const std::string& thumbnailPng) {
    // Slot bound: negative or absurd slots fabricate paths outside the save
    // dir; the UI only uses 0..99. Guard at the boundary.
    if (slot < 0 || slot > 99) {
        fprintf(stderr, "[SaveManager] Slot %d out of range [0..99]\n", slot);
        return false;
    }
    // [R2-FIX] This is the canonical C++ JSON save path (Path A).
    // Legacy Lua serialization path (Path B, scripts/system.lua System.save/load)
    // was removed; all KAG [save]/[load] commands and scripts use the
    // KAG.save_game() / KAG.load_game() bindings which route here.

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
    // Compact JSON (no indentation): saves run frequently (quicksave/auto)
    // and the envelope is re-read by listSaves -- ~40% smaller files.
    std::string jsonStr = envelope.dump();

    bool ok = writeFile(path, jsonStr);
    if (ok) {
        printf("[SaveManager] Saved slot %d (%s, token %d, %zu bytes)\n",
               slot, sceneName.c_str(), tokenIndex, jsonStr.size());
    }
    return ok;
}

// ============================================================================
//  Load (read JSON from disk, return structured data)
// ============================================================================

json SaveManager::load(int slot, SaveMeta* outMeta) {
    if (slot < 0 || slot > 99) {
        fprintf(stderr, "[SaveManager] Slot %d out of range [0..99]\n", slot);
        return {};
    }
    // [R2-FIX] This is the canonical C++ JSON save path (Path A).
    // Legacy Lua serialization path (Path B, scripts/system.lua System.save/load)
    // was removed; all KAG [save]/[load] commands and scripts use the
    // KAG.save_game() / KAG.load_game() bindings which route here.

    std::string contents = readFile(slotPath(slot));
    if (contents.empty()) return json();

    json envelope;
    try {
        envelope = json::parse(contents);
    } catch (const json::exception& e) {
        fprintf(stderr, "[SaveManager] JSON parse error: %s\n", e.what());
        return json();
    }

    // A valid JSON value that is not an object ([] / "x" / 42) would make the
    // value() reads below throw type_error.306 across the Lua C boundary --
    // treat it as corrupt and fail gracefully instead of crashing.
    if (!envelope.is_object()) {
        fprintf(stderr, "[SaveManager] Save envelope is not a JSON object; treating as corrupt\n");
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

    // Scan the bounded slot range (0..99, same as the save/load guard):
    // legacy files beyond 99 would otherwise be enumerated here. NO
    // early break: a consecutive-empty optimization would hide sparse
    // slots (audit: a save at slot 99 vanished from the list when slots
    // 6..13 were empty). 100 stat calls are cheap for a low-frequency
    // list operation.
    for (int slot = 0; slot <= 99; slot++) {
        std::string contents = readFile(slotPath(slot));
        if (contents.empty()) {
            continue;
        }
        json envelope;
        try {
            envelope = json::parse(contents);
        } catch (const json::exception&) {
            continue;
        }
        if (!envelope.is_object()) continue;  // corrupt: not an object envelope

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
    if (slot < 0 || slot > 99) return false;
    return !readFile(slotPath(slot)).empty();
}

bool SaveManager::deleteSlot(int slot) {
    if (slot < 0 || slot > 99) {
        fprintf(stderr, "[SaveManager] Slot %d out of range [0..99]\n", slot);
        return false;
    }
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
    if (toVersion <= fromVersion) {
        fprintf(stderr, "[SaveManager] Rejected migration v%d -> v%d (must increase)\n", fromVersion, toVersion);
        return;
    }
    m_migrations[fromVersion] = {toVersion, fn};
    if (toVersion > m_currentSchemaVersion) {
        m_currentSchemaVersion = toVersion;
    }
    printf("[SaveManager] Registered migration: v%d -> v%d\n", fromVersion, toVersion);
}

json SaveManager::migrate(const json& data, int fromVersion) {
    if (!data.is_object()) {
        fprintf(stderr, "[SaveManager] Migration input is not an object; skipping\n");
        return data;
    }
    json current = data;
    int ver = fromVersion;

    int steps = 0;
    while (steps < 64) {
        auto it = m_migrations.find(ver);
        if (it == m_migrations.end()) break;

        int nextVer = it->second.first;
        printf("[SaveManager] Applying migration v%d -> v%d\n", ver, nextVer);
        current = it->second.second(current);
        ver = nextVer;
        steps++;
    }
    if (steps >= 64) {
        fprintf(stderr, "[SaveManager] Migration chain exceeded 64 steps (cycle?) at v%d\n", ver);
    }
    return current;
}

// ============================================================================
//  Built-in migration scripts
// ============================================================================

void SaveManager::registerBuiltinMigrations() {
    // [R2-FIX] Built-in schema migrations handle format evolution.
    // Current chain: v1->v2(add playtime)->v3(add minigame)->v4(add live2d)->v5(add editor).
    // New fields added in save.lua capture_state() should increment schema_version there.

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
bool SaveManager::s_gfxReady = false;

std::string SaveManager::captureThumbnailPNG(int width, int height) {
    (void)width; (void)height;
    if (!s_gfxReady) {
        fprintf(stderr, "[SaveManager] Thumbnail skipped: gfx not ready\n");
        return "";
    }
    char path[256];
    static int thumbCounter = 0;
    snprintf(path, sizeof(path), "save_thumb_%d.png", thumbCounter++);
    if (thumbCounter > 99) thumbCounter = 0;
    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path);
    bgfx::frame();
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return "";
    std::streamsize size = file.tellg();
    if (size <= 0) return "";
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> buffer(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    if (!file.good()) { file.close(); std::remove(path); return ""; }
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
