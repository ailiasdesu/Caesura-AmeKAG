// ===========================================================================
//  Caesura (AmeKAG) -- SaveManager.h
//  Spec [6.1]: JSON save/load system with schema versioning.
//  Uses nlohmann/json v3.11.3 — structured save data, nested objects,
//  robust serialization of complex game state (Live2D, MiniGame, Editor).
// ===========================================================================

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include "api/ISaveManager.h"

struct lua_State;

namespace Caesura {

class SaveManager : public ISaveManager {
public:
    SaveManager();
    ~SaveManager() override;
    static const char* ENGINE_VERSION;

    SaveManager(const SaveManager&) = delete;
    SaveManager& operator=(const SaveManager&) = delete;

    void init(const std::string& saveDir) override;

    // Save a structured JSON object (engine wraps it with metadata)
    bool save(int slot, const json& gameData,
              const std::string& sceneName,
              int tokenIndex,
              const std::string& thumbnailPng = "") override;

    // Load: returns the "data" sub-object, or empty json on failure
    json load(int slot, SaveMeta* outMeta = nullptr) override;

    std::vector<SaveMeta> listSaves() override;
    bool slotExists(int slot) override;
    bool deleteSlot(int slot) override;

    // Migration
    void registerMigration(int fromVersion, int toVersion, MigrationFn fn) override;
    json migrate(const json& data, int fromVersion);

    int currentSchemaVersion() const override { return m_currentSchemaVersion; }

    // Encryption (AES-256-GCM via CryptoEngine)
    static constexpr uint32_t ENCRYPT_MAGIC = 0x53454143;
    void setEncryptionKey(const uint8_t key[32]) override;
    void clearEncryptionKey() override;
    bool isEncryptionEnabled() const override { return m_keySet; }

    // Thumbnail capture (SU-4 stub — bgfx readback deferred)

    // Pluggable storage provider (SU-6) — default: LocalFileSaveProvider
    void setSaveProvider(std::unique_ptr<class ISaveProvider> provider) override;
    ISaveProvider* getSaveProvider() const override { return m_saveProvider.get(); }
    bool configureCloudSync(const std::string& endpoint) override;
    bool pushSlotToCloud(int slot) override;
    bool pullSlotFromCloud(int slot) override;

    std::string captureThumbnailPNG(int width = 320, int height = 180) override;
    void setGfxReady(bool ready) override { s_gfxReady = ready; }
    bool isGfxReady() const { return s_gfxReady; }
    static bool s_gfxReady;

private:
    std::string m_saveDir;
    int m_currentSchemaVersion = 1;
        bool m_keySet = false;
    uint8_t m_encryptKey[32] = {0};
    std::unique_ptr<ISaveProvider> m_saveProvider;
    std::unordered_map<int, std::pair<int, MigrationFn>> m_migrations;

    std::string slotPath(int slot) const;
    std::string readFile(const std::string& path);
    bool writeFile(const std::string& path, const std::string& content);
    void registerBuiltinMigrations();
};

} // namespace Caesura
