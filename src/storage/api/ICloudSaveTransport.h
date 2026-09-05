#pragma once

#include <string>

namespace Caesura {

// Optional raw transfer capability. The local endpoint is explicit because a
// Steam ISaveProvider uses cloud storage for ordinary readFile/writeFile calls.
// Reads stage bytes without changing either endpoint; writes submit exactly the
// supplied bytes. SaveManager validates the staged bytes before choosing a write.
class ICloudSaveTransport {
public:
    virtual ~ICloudSaveTransport() = default;
    virtual std::string readLocalFile(const std::string& slotPath) = 0;
    virtual bool writeLocalFile(const std::string& slotPath, const std::string& bytes) = 0;
    virtual std::string readCloudFile(const std::string& slotPath) = 0;
    virtual bool writeCloudFile(const std::string& slotPath, const std::string& bytes) = 0;
};

} // namespace Caesura
