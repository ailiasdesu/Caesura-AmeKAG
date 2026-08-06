#include "resource/XP3Archive.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <limits>

// miniz single-header ?? need to define MINIZ_HEADER_FILE_ONLY first
// then include the .c in one TU later (in CMake)
#include <miniz.h>

namespace fs = std::filesystem;

namespace Caesura {

namespace {
constexpr size_t kMaxIndexSize = 256u * 1024u * 1024u;
constexpr uint64_t kMaxExtractedFileSize = 1024ull * 1024ull * 1024ull;
constexpr uint64_t kMaxTotalExtractedSize = 8ull * 1024ull * 1024ull * 1024ull;
constexpr size_t kMaxPathCodeUnits = 4096;
}

// ============================================================================
//  Byte-order helpers (little-endian read/write)
// ============================================================================

static inline void WriteU32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back((uint8_t)(v));
    buf.push_back((uint8_t)(v >> 8));
    buf.push_back((uint8_t)(v >> 16));
    buf.push_back((uint8_t)(v >> 24));
}
static inline void WriteU64(std::vector<uint8_t>& buf, uint64_t v) {
    WriteU32(buf, (uint32_t)(v));
    WriteU32(buf, (uint32_t)(v >> 32));
}
static inline uint32_t ReadU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline uint64_t ReadU64(const uint8_t* p) {
    return (uint64_t)ReadU32(p) | ((uint64_t)ReadU32(p + 4) << 32);
}

// ============================================================================
//  Zlib helpers via miniz
// ============================================================================

static std::vector<uint8_t> ZlibCompress(const uint8_t* data, size_t size, int level = 6) {
    mz_ulong dstLen = mz_compressBound((mz_ulong)size);
    std::vector<uint8_t> out(dstLen);
    if (mz_compress2(out.data(), &dstLen, data, (mz_ulong)size, level) != MZ_OK)
        return {};
    out.resize(dstLen);
    return out;
}

static std::vector<uint8_t> ZlibDecompress(const uint8_t* data, size_t size, size_t expectedOut) {
    mz_ulong dstLen = (mz_ulong)expectedOut;
    std::vector<uint8_t> out(dstLen);
    if (mz_uncompress(out.data(), &dstLen, data, (mz_ulong)size) != MZ_OK)
        return {};
    out.resize(dstLen);
    return out;
}

static bool DecodeIndex(const uint8_t* data, size_t size, std::vector<uint8_t>& output) {
    if (!data || size == 0 || size > kMaxIndexSize) return false;

    size_t capacity = size > kMaxIndexSize / 4 ? kMaxIndexSize : size * 4;
    capacity = std::max<size_t>(capacity, 1024);
    capacity = std::min(capacity, kMaxIndexSize);

    while (true) {
        output.resize(capacity);
        mz_ulong outputSize = static_cast<mz_ulong>(capacity);
        const int result = mz_uncompress(output.data(), &outputSize, data,
                                         static_cast<mz_ulong>(size));
        if (result == MZ_OK) {
            output.resize(static_cast<size_t>(outputSize));
            return true;
        }
        if (result != MZ_BUF_ERROR || capacity == kMaxIndexSize) {
            output.clear();
            return false;
        }
        capacity = std::min(kMaxIndexSize, capacity * 2);
    }
}

// ============================================================================
//  Read entire file into memory
// ============================================================================

static std::vector<uint8_t> ReadFileBytes(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return {};
#ifdef _WIN32
    _fseeki64(f, 0, SEEK_END);
    int64_t sz = _ftelli64(f);
    _fseeki64(f, 0, SEEK_SET);
#else
    fseeko(f, 0, SEEK_END);
    off_t sz = ftello(f);
    fseeko(f, 0, SEEK_SET);
#endif
    if (sz <= 0) { fclose(f); return {}; }
    std::vector<uint8_t> out((size_t)sz);
    size_t bytesRead = fread(out.data(), 1, (size_t)sz, f);
    fclose(f);
    if (bytesRead != (size_t)sz) return {};
    return out;
}

static bool WriteFileBytes(const std::string& path, const uint8_t* data, size_t size) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    if (size > 0) {
        size_t written = fwrite(data, 1, size, f);
        fclose(f);
        return written == size;
    }
    fclose(f);
    return true;
}

// ============================================================================
//  Encode filename as UTF-16LE bytes (null-terminated, 2-byte aligned)
// ============================================================================

static std::vector<uint8_t> EncodeFileName(const std::wstring& name) {
    std::vector<uint8_t> out;
    for (wchar_t ch : name) {
        out.push_back((uint8_t)(ch));
        out.push_back((uint8_t)(ch >> 8));
    }
    // null terminator
    out.push_back(0);
    out.push_back(0);
    // 2-byte align
    if (out.size() % 2 != 0) out.push_back(0);
    return out;
}

static bool ReadU32Checked(const uint8_t*& cursor, const uint8_t* end, uint32_t& value) {
    if (static_cast<size_t>(end - cursor) < sizeof(uint32_t)) return false;
    value = ReadU32(cursor);
    cursor += sizeof(uint32_t);
    return true;
}

static bool ReadU64Checked(const uint8_t*& cursor, const uint8_t* end, uint64_t& value) {
    if (static_cast<size_t>(end - cursor) < sizeof(uint64_t)) return false;
    value = ReadU64(cursor);
    cursor += sizeof(uint64_t);
    return true;
}

static bool DecodeFileName(const uint8_t*& cursor, const uint8_t* end,
                           std::wstring& result) {
    result.clear();
    while (static_cast<size_t>(end - cursor) >= 2) {
        const uint16_t codeUnit = static_cast<uint16_t>(cursor[0]) |
                                  (static_cast<uint16_t>(cursor[1]) << 8);
        cursor += 2;
        if (codeUnit == 0) return true;
        if (result.size() >= kMaxPathCodeUnits) return false;
        result.push_back(static_cast<wchar_t>(codeUnit));
    }
    return false;
}

// ============================================================================
//  Build index buffer
// ============================================================================

static std::vector<uint8_t> BuildIndex(const std::vector<XP3Archive::FileEntry>& files) {
    std::vector<uint8_t> idx;
    for (auto& f : files) {
        WriteU32(idx, f.flags);
        WriteU64(idx, f.orgSize);
        WriteU64(idx, f.arcSize);
        auto nameBytes = EncodeFileName(f.name);
        idx.insert(idx.end(), nameBytes.begin(), nameBytes.end());
        WriteU32(idx, (uint32_t)f.segments.size());
        for (auto& s : f.segments) {
            WriteU32(idx, s.flags);
            WriteU64(idx, s.offset);
            WriteU64(idx, s.orgSize);
            WriteU64(idx, s.arcSize);
        }
    }
    return idx;
}

// ============================================================================
//  Parse index from decompressed buffer
// ============================================================================

static bool ParseIndex(const uint8_t* data, size_t size,
                       std::vector<XP3Archive::FileEntry>& files) {
    files.clear();
    if (!data || size == 0 || size > kMaxIndexSize) return false;

    const uint8_t* cursor = data;
    const uint8_t* end = data + size;
    while (cursor < end) {
        XP3Archive::FileEntry fe;
        if (!ReadU32Checked(cursor, end, fe.flags) ||
            !ReadU64Checked(cursor, end, fe.orgSize) ||
            !ReadU64Checked(cursor, end, fe.arcSize) ||
            !DecodeFileName(cursor, end, fe.name)) {
            return false;
        }

        uint32_t segCount = 0;
        if (!ReadU32Checked(cursor, end, segCount)) return false;
        constexpr size_t kSegmentRecordSize = 4 + 8 + 8 + 8;
        if (segCount == 0 ||
            segCount > static_cast<size_t>(end - cursor) / kSegmentRecordSize) {
            return false;
        }

        uint64_t totalOriginal = 0;
        uint64_t totalArchived = 0;
        for (uint32_t j = 0; j < segCount; ++j) {
            XP3Archive::SegEntry seg;
            if (!ReadU32Checked(cursor, end, seg.flags) ||
                !ReadU64Checked(cursor, end, seg.offset) ||
                !ReadU64Checked(cursor, end, seg.orgSize) ||
                !ReadU64Checked(cursor, end, seg.arcSize)) {
                return false;
            }

            const uint32_t compression = seg.flags & XP3Archive::XP3_SEGM_MASK;
            if (compression != XP3Archive::XP3_ENC_RAW &&
                compression != XP3Archive::XP3_ENC_ZLIB) {
                return false;
            }
            if (compression == XP3Archive::XP3_ENC_RAW && seg.orgSize != seg.arcSize) {
                return false;
            }
            if (seg.orgSize > kMaxExtractedFileSize ||
                totalOriginal > std::numeric_limits<uint64_t>::max() - seg.orgSize ||
                totalArchived > std::numeric_limits<uint64_t>::max() - seg.arcSize) {
                return false;
            }
            totalOriginal += seg.orgSize;
            totalArchived += seg.arcSize;
            fe.segments.push_back(seg);
        }
        if (fe.name.empty() || totalOriginal != fe.orgSize || totalArchived != fe.arcSize ||
            fe.orgSize > kMaxExtractedFileSize) {
            return false;
        }
        files.push_back(std::move(fe));
    }
    return cursor == end;
}

static bool ValidateEntries(const std::vector<XP3Archive::FileEntry>& files,
                            uint64_t indexOffset) {
    uint64_t totalExtracted = 0;
    for (const auto& file : files) {
        if (totalExtracted > kMaxTotalExtractedSize - file.orgSize) return false;
        totalExtracted += file.orgSize;
        for (const auto& segment : file.segments) {
            if (segment.offset < 13 || segment.offset > indexOffset ||
                segment.arcSize > indexOffset - segment.offset) {
                return false;
            }
        }
    }
    return true;
}

static bool ResolveOutputPath(const fs::path& outputRoot, const std::wstring& name,
                              fs::path& resolved) {
    std::string relativePath(name.begin(), name.end());
    std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
    fs::path relative(relativePath);
    if (relative.empty() || relative.is_absolute() || relative.has_root_name()) return false;
    for (const auto& part : relative) {
        if (part == "..") return false;
    }

    resolved = (outputRoot / relative).lexically_normal();
    const fs::path checkedRelative = resolved.lexically_relative(outputRoot);
    if (checkedRelative.empty() || checkedRelative.is_absolute()) return false;
    for (const auto& part : checkedRelative) {
        if (part == "..") return false;
    }
    return true;
}

// ============================================================================
//  pack ?? directory ?? XP3
// ============================================================================

bool XP3Archive::pack(const std::string& inputDir, const std::string& outputFile,
                      std::function<void(int,int)> progressCb) {
    FILE* out = fopen(outputFile.c_str(), "wb");
    if (!out) {
        fprintf(stderr, "[XP3] Cannot open output: %s\n", outputFile.c_str());
        return false;
    }

    // Collect files recursively
    std::vector<FileEntry> files;
    std::string dirAbs = fs::absolute(inputDir).string();
    for (auto& de : fs::recursive_directory_iterator(inputDir)) {
        if (!de.is_regular_file()) continue;
        std::string absPath = de.path().string();
        std::string relPath = absPath.substr(dirAbs.size() + 1);
        // normalize backslashes
        std::replace(relPath.begin(), relPath.end(), '\\', '/');

        auto raw = ReadFileBytes(absPath);
        if (raw.empty()) continue;

        // Compress with zlib
        auto compressed = ZlibCompress(raw.data(), raw.size());
        bool useZlib = !compressed.empty() && compressed.size() < raw.size();
        auto& finalData = useZlib ? compressed : raw;

        FileEntry fe;
        fe.name = std::wstring(relPath.begin(), relPath.end());
        fe.flags = 0;
        fe.orgSize = raw.size();
        fe.arcSize = finalData.size();
        fe.data = finalData;

        SegEntry seg;
        seg.flags   = useZlib ? XP3_ENC_ZLIB : XP3_ENC_RAW;
        seg.offset  = 0; // filled later
        seg.orgSize = raw.size();
        seg.arcSize = finalData.size();
        fe.segments.push_back(seg);

        files.push_back(std::move(fe));
    }

    int total = (int)files.size();

    // Write XP3 header placeholder
    const char magic[] = "XP3\r\n";
    size_t magicLen = 5;
    uint64_t placeholderOff = 0;
    fwrite(magic, 1, magicLen, out);
    fwrite(&placeholderOff, 8, 1, out); // placeholder

    // Write file data segments
    uint64_t dataOffset = magicLen + 8;
    for (int i = 0; i < total; ++i) {
        auto& fe = files[i];
        fe.segments[0].offset = dataOffset;
        fwrite(fe.data.data(), 1, fe.data.size(), out);
        dataOffset += fe.data.size();
        if (progressCb) progressCb(i + 1, total);
    }

    // Build index
    uint64_t indexOffset = dataOffset;
    auto rawIndex = BuildIndex(files);
    auto zlibIndex = ZlibCompress(rawIndex.data(), rawIndex.size());
    if (zlibIndex.empty()) {
        fprintf(stderr, "[XP3] Index compression failed.\n");
        fclose(out);
        return false;
    }

    // Write compressed index
    fwrite(zlibIndex.data(), 1, zlibIndex.size(), out);

    // Seek back to write actual index offset
    fseek(out, static_cast<long>(magicLen), SEEK_SET);
    fwrite(&indexOffset, 8, 1, out);
    fclose(out);

    printf("[XP3] Packed %d files ?? %s (%.2f MB)\n",
           total, outputFile.c_str(), (double)indexOffset / (1024.0 * 1024.0));
    return true;
}

// ============================================================================
//  unpack ?? XP3 ?? directory
// ============================================================================

bool XP3Archive::unpack(const std::string& xp3File, const std::string& outputDir,
                        std::function<void(int,int)> progressCb) {
    auto raw = ReadFileBytes(xp3File);
    if (raw.size() < 13) {
        fprintf(stderr, "[XP3] File too small: %s\n", xp3File.c_str());
        return false;
    }

    // Verify magic
    const char magic[] = "XP3\r\n";
    if (memcmp(raw.data(), magic, 5) != 0) {
        fprintf(stderr, "[XP3] Not an XP3 archive: %s\n", xp3File.c_str());
        return false;
    }

    uint64_t indexOff = ReadU64(raw.data() + 5);
    if (indexOff < 13 || indexOff >= raw.size()) {
        fprintf(stderr, "[XP3] Invalid index offset.\n");
        return false;
    }
    const size_t indexOffset = static_cast<size_t>(indexOff);

    // Read index segment (from indexOff to end of file)
    size_t indexRawSize = raw.size() - indexOffset;

    // M1: prevent decompression bomb (cap at 256 MB)
    if (indexRawSize > kMaxIndexSize) {
        fprintf(stderr, "[XP3] Index too large (%.1f MB > 256 MB limit).\n",
                (double)indexRawSize / (1024.0 * 1024.0));
        return false;
    }

    // Decompress index (try zlib first, then assume raw)
    std::vector<uint8_t> decompIndex;
    const bool indexCompressed = DecodeIndex(raw.data() + indexOffset, indexRawSize, decompIndex);
    const uint8_t* idxData = indexCompressed ? decompIndex.data() : raw.data() + indexOffset;
    const size_t idxSize = indexCompressed ? decompIndex.size() : indexRawSize;

    std::vector<FileEntry> files;
    if (!ParseIndex(idxData, idxSize, files) || !ValidateEntries(files, indexOff)) {
        fprintf(stderr, "[XP3] Invalid index contents.\n");
        return false;
    }
    int total = (int)files.size();

    const fs::path outputRoot = fs::absolute(outputDir).lexically_normal();
    std::vector<fs::path> outputPaths;
    outputPaths.reserve(files.size());
    for (const auto& file : files) {
        fs::path outputPath;
        if (!ResolveOutputPath(outputRoot, file.name, outputPath)) {
            fprintf(stderr, "[XP3] Rejected unsafe output path.\n");
            return false;
        }
        outputPaths.push_back(std::move(outputPath));
    }

    std::error_code ec;
    fs::create_directories(outputRoot, ec);
    if (ec) return false;

    for (int i = 0; i < total; ++i) {
        auto& fe = files[i];
        const fs::path& outputPath = outputPaths[static_cast<size_t>(i)];
        if (outputPath.has_parent_path()) {
            fs::create_directories(outputPath.parent_path(), ec);
            if (ec) return false;
        }

        // Extract segments
        std::vector<uint8_t> outData;
        outData.reserve(static_cast<size_t>(fe.orgSize));
        for (auto& seg : fe.segments) {
            const size_t segmentOffset = static_cast<size_t>(seg.offset);
            const size_t archivedSize = static_cast<size_t>(seg.arcSize);
            const size_t originalSize = static_cast<size_t>(seg.orgSize);
            const uint8_t* segData = raw.data() + segmentOffset;
            bool compressed = (seg.flags & XP3_SEGM_MASK) == XP3_ENC_ZLIB;

            if (compressed) {
                auto d = ZlibDecompress(segData, archivedSize, originalSize);
                if (d.size() != originalSize) return false;
                outData.insert(outData.end(), d.begin(), d.end());
            } else {
                outData.insert(outData.end(), segData, segData + archivedSize);
            }
        }
        if (outData.size() != fe.orgSize) return false;

        if (!WriteFileBytes(outputPath.string(), outData.data(), outData.size())) return false;
        if (progressCb) progressCb(i + 1, total);
    }

    printf("[XP3] Unpacked %d files ?? %s\n", total, outputDir.c_str());
    return true;
}

// ============================================================================
//  list ?? read file list without extraction
// ============================================================================

std::vector<XP3Archive::FileEntry> XP3Archive::list(const std::string& xp3File) {
    auto raw = ReadFileBytes(xp3File);
    if (raw.size() < 13) return {};

    const char magic[] = "XP3\r\n";
    if (memcmp(raw.data(), magic, 5) != 0) return {};

    uint64_t indexOff = ReadU64(raw.data() + 5);
    if (indexOff < 13 || indexOff >= raw.size()) return {};
    const size_t indexOffset = static_cast<size_t>(indexOff);
    size_t indexRawSize = raw.size() - indexOffset;

    // M1: prevent decompression bomb (cap at 256 MB) →→ same as unpack()
    if (indexRawSize > kMaxIndexSize) return {};
    std::vector<uint8_t> decompIndex;
    const bool indexCompressed = DecodeIndex(raw.data() + indexOffset, indexRawSize, decompIndex);
    const uint8_t* idxData = indexCompressed ? decompIndex.data() : raw.data() + indexOffset;
    const size_t idxSize = indexCompressed ? decompIndex.size() : indexRawSize;

    std::vector<FileEntry> files;
    if (!ParseIndex(idxData, idxSize, files) || !ValidateEntries(files, indexOff)) return {};
    return files;
}

} // namespace Caesura
