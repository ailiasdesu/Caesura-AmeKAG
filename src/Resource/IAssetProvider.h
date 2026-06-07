// IAssetProvider — abstract interface for asset sources
#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace caesura {

class IAssetProvider {
public:
    virtual ~IAssetProvider() = default;

    // Read a file. Returns empty vector if not found or on error.
    virtual std::vector<uint8_t> read(const std::string& path) = 0;

    // Check if a file exists in this provider.
    virtual bool exists(const std::string& path) = 0;

    // Human-readable source name for debugging.
    virtual std::string getSource() const = 0;

    // Priority: higher = checked first. CARC=10, Dir=5, Patch=8.
    virtual int priority() const = 0;

    // Verify integrity of the asset source. Returns true if valid.
    // CARC providers verify Ed25519 signature; others return true.
    virtual bool verify() = 0;
};

} // namespace caesura