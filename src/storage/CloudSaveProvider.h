// CloudSaveProvider — ISaveProvider backed by ISteamRemoteStorage
// Splits saves > 256KB into chunks (Steam Remote Storage per-file limit)
#pragma once
#include "api/ISaveProvider.h"
#include "api/ICloudSaveTransport.h"
#include <cstdint>  // fixed-width types (GCC strict)

namespace Caesura {
class ISteamBackend;

class CloudSaveProvider : public ISaveProvider, public ICloudSaveTransport {
public:
    explicit CloudSaveProvider(ISteamBackend* steam);
    ~CloudSaveProvider() override = default;

    // Paths are normalized to a FLAT cloud key (directory component stripped),
    // so "<saveDir>/save_5.json" and "save_5.json" address the SAME cloud
    // object whichever entry point is used.
    std::string readFile(const std::string& path) override;
    bool writeFile(const std::string& path, const std::string& content) override;
    bool deleteFile(const std::string& path) override;
    std::vector<std::string> listFiles(const std::string& pattern) override;

    // Cloud sync overrides
    bool pushToCloud(const std::string& slotPath) override;
    bool pullFromCloud(const std::string& slotPath) override;
    bool supportsCloudSync() const override { return true; }
    std::string readLocalFile(const std::string& slotPath) override;
    bool writeLocalFile(const std::string& slotPath, const std::string& bytes) override;
    std::string readCloudFile(const std::string& slotPath) override;
    bool writeCloudFile(const std::string& slotPath, const std::string& bytes) override;

private:
    // Flat cloud key for a slot path (directory component stripped).
    static std::string cloudKey(const std::string& slotPath);

    ISteamBackend* m_steam;
    static constexpr int32_t kChunkSize = 256 * 1024; // 256KB Steam limit
    // Hard cap on a single chunked save; protects against corrupt .meta
    // triggering multi-GB reserves / billion-iteration loops.
    static constexpr int32_t kMaxChunkedSize = 64 * 1024 * 1024; // 64MB
};

} // namespace Caesura
