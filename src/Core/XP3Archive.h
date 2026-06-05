#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace Caesura {

// ============================================================================
// XP3Archive ¡ª Kirikiri XP3 archive packer / unpacker
// ============================================================================
// Format (simplified from krkrz):
//   [Header ] "XP3\r\n" magic (5 bytes) + index_offset (8 bytes LE)
//   [Data   ] raw/zlib-compressed file segments, concatenated
//   [Index  ] zlib-compressed TOC (file list with segment info)
//   [End    ] data ends at index_offset (from start of file)
//
// Index encoding (after zlib decompression):
//   For each file:
//     u32 flags (bit31 = protected)
//     u64 orgSize
//     u64 arcSize
//     UTF-16LE filename, null-terminated, 2-byte aligned
//     u32 segmentCount
//     For each segment:
//       u32 flags (lower 3 bits = compression: 0=raw, 1=zlib)
//       u64 offset (from archive start)
//       u64 orgSize
//       u64 arcSize

class XP3Archive {
public:
    static constexpr uint32_t XP3_ENC_RAW      = 0;
    static constexpr uint32_t XP3_ENC_ZLIB     = 1;
    static constexpr uint32_t XP3_SEGM_MASK    = 0x07;
    static constexpr uint32_t XP3_FILE_PROTECTED = (1u << 31);

    struct SegEntry {
        uint32_t flags   = 0;
        uint64_t offset  = 0;
        uint64_t orgSize = 0;
        uint64_t arcSize = 0;
    };

    struct FileEntry {
        std::wstring name;
        uint32_t     flags   = 0;
        uint64_t     orgSize = 0;
        uint64_t     arcSize = 0;
        std::vector<SegEntry> segments;
        std::vector<uint8_t>  data; // loaded payload for new writes
    };

    // -- Pack: compress directory into XP3 ---------------------------------
    // progressCb(current, total) called per file
    static bool pack(const std::string& inputDir,
                     const std::string& outputFile,
                     std::function<void(int,int)> progressCb = nullptr);

    // -- Unpack: extract XP3 to directory ----------------------------------
    static bool unpack(const std::string& xp3File,
                       const std::string& outputDir,
                       std::function<void(int,int)> progressCb = nullptr);

    // -- List files in archive (no extraction) -----------------------------
    static std::vector<FileEntry> list(const std::string& xp3File);
};

} // namespace Caesura