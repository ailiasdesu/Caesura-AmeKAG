#pragma once

#include "api/ISaveProvider.h"

namespace Caesura {

class LocalFileSaveProvider final : public ISaveProvider {
public:
    std::string readFile(const std::string& path) override;
    bool writeFile(const std::string& path, const std::string& content) override;
    bool deleteFile(const std::string& path) override;
    std::vector<std::string> listFiles(const std::string& pattern) override;
    bool pushToCloud(const std::string& slotPath) override;
    bool pullFromCloud(const std::string& slotPath) override;
    bool supportsCloudSync() const override { return false; }
};

} // namespace Caesura
