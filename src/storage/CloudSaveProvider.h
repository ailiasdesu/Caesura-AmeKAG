// CloudSaveProvider — ISaveProvider backed by ISteamRemoteStorage
// Splits saves > 256KB into chunks (Steam Remote Storage per-file limit)
#pragma once
#include "ISaveProvider.h"

namespace Caesura {
class ISteamBackend;

class CloudSaveProvider : public ISaveProvider {
public:
    explicit CloudSaveProvider(ISteamBackend* steam);
    ~CloudSaveProvider() override = default;

    std::string readFile(const std::string& path) override;
    bool writeFile(const std::string& path, const std::string& content) override;
    bool deleteFile(const std::string& path) override;
    std::vector<std::string> listFiles(const std::string& pattern) override;

    // Cloud sync overrides
    bool pushToCloud(const std::string& slotPath) override;
    bool pullFromCloud(const std::string& slotPath) override;
    bool supportsCloudSync() const override { return true; }

private:
    ISteamBackend* m_steam;
    static constexpr int32_t kChunkSize = 256 * 1024; // 256KB Steam limit
};

} // namespace Caesura
