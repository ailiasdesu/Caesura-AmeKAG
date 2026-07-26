// ISaveManager - pure virtual interface for save/load management
// Concrete: SaveManager. Pattern: module api/ directory.
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <memory>
#include <nlohmann_json.hpp>

namespace Caesura {

using json = nlohmann::json;

struct SaveMeta {
    int         slot = 0;
    uint64_t    timestamp = 0;
    std::string sceneName;
    std::string thumbnail;
    int         tokenIndex = 1;
    int         schemaVersion = 1;
};

using MigrationFn = std::function<json(json)>;

class ISaveManager {
public:
    virtual ~ISaveManager() = default;

    virtual void init(const std::string& saveDir) = 0;
    virtual bool save(int slot, const json& gameData,
                      const std::string& sceneName,
                      int tokenIndex,
                      const std::string& thumbnailPng = "") = 0;
    virtual json load(int slot, SaveMeta* outMeta = nullptr) = 0;
    virtual std::vector<SaveMeta> listSaves() = 0;
    virtual bool slotExists(int slot) = 0;
    virtual bool deleteSlot(int slot) = 0;
    virtual void setEncryptionKey(const uint8_t key[32]) = 0;
    virtual void clearEncryptionKey() = 0;
    virtual bool isEncryptionEnabled() const = 0;
    virtual void setSaveProvider(std::unique_ptr<class ISaveProvider> provider) = 0;
    virtual class ISaveProvider* getSaveProvider() const = 0;
    virtual int currentSchemaVersion() const = 0;
    virtual void registerMigration(int fromVersion, int toVersion,
                                   MigrationFn fn) = 0;
    virtual std::string captureThumbnailPNG(int width = 320, int height = 180) = 0;
};

} // namespace Caesura
