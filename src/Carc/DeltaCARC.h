// DeltaCARC — differential update format for .carc archives
// Uses file-level comparison: changed/added files copied, removed files listed.
// Entire delta is AES-256-GCM encrypted + Ed25519 signed.
#pragma once
#include "CARCFormat.h"
#include <vector>
#include <string>
#include <cstdint>

namespace Caesura::carc {

// Delta entry flags
enum class DeltaFlag : uint8_t { Remove = 0, Add = 1, Replace = 2 };

// --- Delta Header (80 bytes) ---
inline constexpr uint32_t DELTA_MAGIC = 0x4341524B; // 'CARK' (Caesura Delta)
inline constexpr uint32_t DELTA_VERSION = 1;

#pragma pack(push, 1)
struct DeltaHeader {
    uint32_t magic;                      // 4
    uint32_t version;                    // 4  (8)
    uint8_t  sourceSHA[PATH_HASH_SIZE];  // 32 (40)
    uint8_t  targetSHA[PATH_HASH_SIZE];  // 32 (72)
    uint32_t entryCount;                 // 4  (76)
    uint8_t  reserved[4];               // 4  (80)
};
static_assert(sizeof(DeltaHeader) == 80, "DeltaHeader must be 80 bytes");
#pragma pack(pop)

// --- Delta API ---
class DeltaCARC {
public:
    // Generate delta: oldPath → newPath, write to deltaPath.
    // Returns false on failure.
    static bool generate(const std::string& oldPath,
                         const std::string& newPath,
                         const std::string& deltaPath);

    // Apply delta to source CARC, producing output CARC.
    // Verifies SHA-256 of output against delta header.
    static bool apply(const std::string& sourcePath,
                      const std::string& deltaPath,
                      const std::string& outputPath);

    // Verify delta integrity (signature + hash chain).
    static bool verify(const std::string& deltaPath);
};

} // namespace Caesura::carc