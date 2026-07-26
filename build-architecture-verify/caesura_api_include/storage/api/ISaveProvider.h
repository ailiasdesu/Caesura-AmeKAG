// ===========================================================================
//  Caesura (AmeKAG) — ISaveProvider.h  (SU-6)
//  Abstract save storage provider — enables cloud sync, remote saves, etc.
//  Default implementation: LocalFileSaveProvider (std::ifstream).
// ===========================================================================

#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace Caesura {

// Abstract save storage backend
class ISaveProvider {
public:
    virtual ~ISaveProvider() = default;

    // Read raw bytes from a save slot path
    virtual std::string readFile(const std::string& path) = 0;

    // Write raw bytes to a save slot path
    virtual bool writeFile(const std::string& path, const std::string& content) = 0;

    // Delete a save file
    virtual bool deleteFile(const std::string& path) = 0;

    // List all save files matching a pattern (e.g., "save_*.json")
    virtual std::vector<std::string> listFiles(const std::string& pattern) = 0;

    // Cloud sync operations are explicit adapter capabilities.
    virtual bool pushToCloud(const std::string& slotPath) = 0;

    // Cloud sync: pull saves from remote (no-op by default)
    virtual bool pullFromCloud(const std::string& slotPath) = 0;

    // Check if provider supports cloud sync
    virtual bool supportsCloudSync() const = 0;
};

} // namespace Caesura
