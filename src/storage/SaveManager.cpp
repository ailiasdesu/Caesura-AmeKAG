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
#include "HttpCloudSaveProvider.h"
#include "LocalFileSaveProvider.h"
#include "../di/BackendRegistry.h"
#include "../archive/api/ICryptoEngine.h"
#include "../debug/api/DebugLog.h"
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

// Cloud sync (C7): swap the provider for an HTTP-backed one (keeps the
// local files as the offline source of truth), push/pull a slot's file.
bool SaveManager::configureCloudSync(const std::string& endpoint) {
    if (endpoint.empty()) {
        m_saveProvider = std::make_unique<LocalFileSaveProvider>();
        return true;
    }
    m_saveProvider = std::make_unique<HttpCloudSaveProvider>(endpoint);
    printf("[SaveManager] Cloud sync configured: %s\n", endpoint.c_str());
    return true;
}

bool SaveManager::pushSlotToCloud(int slot) {
    if (!m_saveProvider) return false;
    return m_saveProvider->pushToCloud(slotPath(slot));
}

bool SaveManager::pullSlotFromCloud(int slot) {
    if (!m_saveProvider) return false;
    return m_saveProvider->pullFromCloud(slotPath(slot));
}

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
            DEBUG_ERR(SubSys::Storage, ErrCode::Storage_CryptoFailed, "[SaveManager] Encrypted save too short");
            return "";
        }
        const auto* nonce = reinterpret_cast<const uint8_t*>(content.data() + 4);
        const auto* tag   = reinterpret_cast<const uint8_t*>(content.data() + 16);
        const auto* ct    = reinterpret_cast<const uint8_t*>(content.data() + 32);
        size_t ctLen = content.size() - 32;

        auto* crypto = BackendRegistry::instance().getCryptoEngine();
        if (!crypto) {
            DEBUG_ERR(SubSys::Storage, ErrCode::Storage_CryptoFailed, "[SaveManager] CryptoEngine not initialized");
            return "";
        }
        auto plain = crypto->decrypt(ct, ctLen, m_encryptKey, 32, nonce, 12, tag, 16);
        if (plain.empty()) {
            DEBUG_ERR(SubSys::Storage, ErrCode::Storage_CryptoFailed, "[SaveManager] Decryption failed (wrong key or corrupted data)");
            return "";
        }
        return std::string(reinterpret_cast<char*>(plain.data()), plain.size());
    }

    return content;
}

bool SaveManager::writeFile(const std::string& path, const std::string& content) {
    if (m_saveProvider) return m_saveProvider->writeFile(path, content);

    // Symmetric to readFile()'s MAX_SAVE_SIZE guard: reject oversized payloads
    // up front so they never reach disk as a file that load()/slotExists()/
    // listSaves() would silently skip. Also refuses the write when even a
    // smaller payload would balloon past the ceiling (e.g. +32 B encryption
    // header), keeping the on-disk size always loadable.
    if (content.size() > MAX_SAVE_SIZE) {
        DEBUG_ERR(SubSys::Storage, ErrCode::Storage_SaveWriteFailed,
                  "[SaveManager] Rejecting write to %s: payload %zu bytes exceeds MAX_SAVE_SIZE (%zu)",
                  path.c_str(), content.size(), MAX_SAVE_SIZE);
        return false;
    }

    std::string dataToWrite;
    if (m_keySet) {
        uint8_t nonce[12];
        uint8_t tag[16];
        auto* crypto = BackendRegistry::instance().getCryptoEngine();
        if (!crypto) {
            DEBUG_ERR(SubSys::Storage, ErrCode::Storage_CryptoFailed, "[SaveManager] CryptoEngine not initialized");
            return false;
        }
        crypto->generateNonce(nonce, 12);

        auto cipher = crypto->encrypt(            reinterpret_cast<const uint8_t*>(content.data()), content.size(),
            m_encryptKey, 32, nonce, 12, tag, 16);
        if (cipher.empty()) {
            DEBUG_ERR(SubSys::Storage, ErrCode::Storage_CryptoFailed, "[SaveManager] Encryption failed");
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

    // Mirror readFile()'s file-size check on the exact bytes that would hit
    // disk (encrypted output grows by the 4+12+16 header): any payload the
    // manager would refuse to read back must not be written in the first place.
    if (dataToWrite.size() > MAX_SAVE_SIZE) {
        DEBUG_ERR(SubSys::Storage, ErrCode::Storage_SaveWriteFailed,
                  "[SaveManager] Rejecting write to %s: on-disk size %zu bytes exceeds MAX_SAVE_SIZE (%zu)",
                  path.c_str(), dataToWrite.size(), MAX_SAVE_SIZE);
        return false;
    }

    // Atomic write: write to a temp file next to the target, flush, then
    // rename. A crash mid-write leaves the previous save intact instead of
    // truncating it (rename is atomic on the same filesystem).
    const std::string tmpPath = path + ".tmp";
    std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        DEBUG_ERR(SubSys::Storage, ErrCode::Storage_SaveWriteFailed, "[SaveManager] Failed to open file for writing: %s", tmpPath.c_str());
        return false;
    }
    out.write(dataToWrite.c_str(), static_cast<std::streamsize>(dataToWrite.size()));
    out.flush();
    if (!out.good()) {
        DEBUG_ERR(SubSys::Storage, ErrCode::Storage_SaveWriteFailed, "[SaveManager] Write failed for %s", tmpPath.c_str());
        std::filesystem::remove(tmpPath);  // no stale partial temp file
        return false;
    }
    out.close();
    std::error_code ec;
    std::filesystem::rename(tmpPath, path, ec);
    if (ec) {
        DEBUG_ERR(SubSys::Storage, ErrCode::Storage_SaveWriteFailed, "[SaveManager] Rename failed: %s", ec.message().c_str());
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
        DEBUG_ERR(SubSys::Storage, ErrCode::Ok, "[SaveManager] Slot %d out of range [0..99]", slot);
        return false;
    }
    // [R2-FIX] This is the canonical C++ JSON save path (Path A).
    // Legacy Lua serialization path (Path B, scripts/system.lua System.save/load)
    // was removed; all KAG [save]/[load] commands and scripts use the
    // KAG.save_game() / KAG.load_game() bindings which route here.

    if (m_saveDir.empty()) {
        DEBUG_ERR(SubSys::Storage, ErrCode::Ok, "[SaveManager] Not initialized; call init() first.");
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
        DEBUG_ERR(SubSys::Storage, ErrCode::Ok, "[SaveManager] Slot %d out of range [0..99]", slot);
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
        DEBUG_ERR(SubSys::Storage, ErrCode::Storage_SaveReadFailed, "[SaveManager] JSON parse error: %s", e.what());
        return json();
    }

    // A valid JSON value that is not an object ([] / "x" / 42) would make the
    // value() reads below throw type_error.306 across the Lua C boundary --
    // treat it as corrupt and fail gracefully instead of crashing.
    if (!envelope.is_object()) {
        DEBUG_ERR(SubSys::Storage, ErrCode::Storage_SaveReadFailed, "[SaveManager] Save envelope is not a JSON object; treating as corrupt");
        return json();
    }

    // Field reads must be type-safe: value() throws type_error 302 when a key
    // exists with a wrong type (review ST-1) -- guard each read so a single
    // tampered/corrupt save degrades gracefully instead of crashing.
    auto safeStr = [&envelope](const char* key) -> std::string {
        const auto it = envelope.find(key);
        if (it == envelope.end() || !it->is_string()) return "";
        return it->get<std::string>();
    };
    auto safeUint = [&envelope](const char* key, uint64_t fallback) -> uint64_t {
        const auto it = envelope.find(key);
        if (it == envelope.end() || !it->is_number_unsigned()) return fallback;
        return it->get<uint64_t>();
    };
    auto safeInt = [&envelope](const char* key, int fallback) -> int {
        const auto it = envelope.find(key);
        if (it == envelope.end() || !it->is_number_integer()) return fallback;
        return it->get<int>();
    };
    uint64_t ts       = safeUint("timestamp", 0);
    std::string scene = safeStr("scene");
    int tokenIdx      = safeInt("token_index", 0);
    std::string thumb = safeStr("thumbnail");
    int schemaVer     = safeInt("schema_version", 1);
    std::string engineVer = safeStr("engine_version");

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
        const auto si = envelope.find("timestamp");
        meta.timestamp     = (si != envelope.end() && si->is_number_unsigned()) ? si->get<uint64_t>() : 0;
        const auto sn = envelope.find("scene");
        meta.sceneName     = (sn != envelope.end() && sn->is_string()) ? sn->get<std::string>() : "";
        const auto st = envelope.find("thumbnail");
        meta.thumbnail     = (st != envelope.end() && st->is_string()) ? st->get<std::string>() : "";
        const auto sk = envelope.find("token_index");
        meta.tokenIndex    = (sk != envelope.end() && sk->is_number_integer()) ? sk->get<int>() : 0;
        const auto sv = envelope.find("schema_version");
        meta.schemaVersion = (sv != envelope.end() && sv->is_number_integer()) ? sv->get<int>() : 1;

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
        DEBUG_ERR(SubSys::Storage, ErrCode::Ok, "[SaveManager] Slot %d out of range [0..99]", slot);
        return false;
    }
    std::string path = slotPath(slot);
    if (m_saveProvider) return m_saveProvider->deleteFile(path);
    if (remove(path.c_str()) == 0) {
        printf("[SaveManager] Deleted slot %d\n", slot);
        return true;
    }
    DEBUG_ERR(SubSys::Storage, ErrCode::Ok, "[SaveManager] Failed to delete slot %d", slot);
    return false;
}

// ============================================================================
//  Migration (with structured JSON)
// ============================================================================

void SaveManager::registerMigration(int fromVersion, int toVersion, MigrationFn fn) {
    if (toVersion <= fromVersion) {
        DEBUG_ERR(SubSys::Storage, ErrCode::Ok, "[SaveManager] Rejected migration v%d -> v%d (must increase)", fromVersion, toVersion);
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
        DEBUG_ERR(SubSys::Storage, ErrCode::Ok, "[SaveManager] Migration input is not an object; skipping");
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
        DEBUG_ERR(SubSys::Storage, ErrCode::Ok, "[SaveManager] Migration chain exceeded 64 steps (cycle?) at v%d", ver);
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
        DEBUG_ERR(SubSys::Storage, ErrCode::Ok, "[SaveManager] Thumbnail skipped: gfx not ready");
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
