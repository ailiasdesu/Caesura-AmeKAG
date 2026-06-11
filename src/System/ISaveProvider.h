// ===========================================================================
//  Caesura (AmeKAG) — ISaveProvider.h  (SU-6)
//  Abstract save storage provider — enables cloud sync, remote saves, etc.
//  Default implementation: LocalFileSaveProvider (std::ifstream).
// ===========================================================================

#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

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

    // Cloud sync: push saves to remote (no-op by default)
    virtual bool pushToCloud(const std::string& slotPath) { (void)slotPath; return false; }

    // Cloud sync: pull saves from remote (no-op by default)
    virtual bool pullFromCloud(const std::string& slotPath) { (void)slotPath; return false; }

    // Check if provider supports cloud sync
    virtual bool supportsCloudSync() const { return false; }
};

// Default: local filesystem via std::ifstream/std::ofstream
class LocalFileSaveProvider : public ISaveProvider {
public:
    std::string readFile(const std::string& path) override;
    bool writeFile(const std::string& path, const std::string& content) override;
    bool deleteFile(const std::string& path) override;
    std::vector<std::string> listFiles(const std::string& pattern) override;
};

} // namespace Caesura