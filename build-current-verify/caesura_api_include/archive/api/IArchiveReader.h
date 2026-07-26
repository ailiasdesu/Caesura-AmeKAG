// IArchiveReader — pure virtual interface for archive reading
// Concrete: CARCReader. Pattern: module api/ directory.
#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace Caesura::carc {

class IArchiveReader {
public:
    virtual ~IArchiveReader() = default;

    virtual bool open(const std::string& path, const std::string& pubKeyPath = "") = 0;
    virtual void close() = 0;
    virtual std::vector<uint8_t> readFile(const std::string& relativePath) = 0;
    virtual bool hasFile(const std::string& relativePath) const = 0;
    virtual size_t numFiles() const = 0;
    virtual bool isOpen() const = 0;
};

} // namespace Caesura::carc
