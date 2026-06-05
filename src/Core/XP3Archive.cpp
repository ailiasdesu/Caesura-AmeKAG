#include "XP3Archive.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <filesystem>

// miniz single-header ?? need to define MINIZ_HEADER_FILE_ONLY first
// then include the .c in one TU later (in CMake)
#include <miniz.h>

namespace fs = std::filesystem;

namespace Caesura {

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
    fread(out.data(), 1, (size_t)sz, f);
    fclose(f);
    return out;
}

static bool WriteFileBytes(const std::string& path, const uint8_t* data, size_t size) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    if (size > 0) fwrite(data, 1, size, f);
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

static std::wstring DecodeFileName(const uint8_t*& p) {
    std::wstring result;
    while (true) {
        wchar_t ch = (wchar_t)ReadU32(p);
        if (ch == 0) break;
        result += ch;
        p += 2;
    }
    p += 2; // skip null terminator
    // byte align: skip padding byte if present
    return result;
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

static std::vector<XP3Archive::FileEntry> ParseIndex(const uint8_t* data, size_t size) {
    std::vector<XP3Archive::FileEntry> files;
    const uint8_t* end = data + size;
    while (data < end) {
        XP3Archive::FileEntry fe;
        fe.flags   = ReadU32(data);  data += 4;
        fe.orgSize = ReadU64(data);  data += 8;
        fe.arcSize = ReadU64(data);  data += 8;
        fe.name = DecodeFileName(data);
        uint32_t segCount = ReadU32(data); data += 4;
        for (uint32_t j = 0; j < segCount; ++j) {
            XP3Archive::SegEntry seg;
            seg.flags   = ReadU32(data);  data += 4;
            seg.offset  = ReadU64(data);  data += 8;
            seg.orgSize = ReadU64(data);  data += 8;
            seg.arcSize = ReadU64(data);  data += 8;
            fe.segments.push_back(seg);
        }
        files.push_back(std::move(fe));
    }
    return files;
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
    fseek(out, magicLen, SEEK_SET);
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

    // Read index segment (from indexOff to end of file)
    size_t indexRawSize = raw.size() - (size_t)indexOff;
    if (indexOff >= raw.size() || indexRawSize == 0) {
        fprintf(stderr, "[XP3] Invalid index offset.\n");
        return false;
    }

    // M1: prevent decompression bomb (cap at 256 MB)
    constexpr size_t kMaxIndexSize = 256 * 1024 * 1024;
    if (indexRawSize > kMaxIndexSize) {
        fprintf(stderr, "[XP3] Index too large (%.1f MB > 256 MB limit).\n",
                (double)indexRawSize / (1024.0 * 1024.0));
        return false;
    }

    // Decompress index (try zlib first, then assume raw)
    size_t estimateOut = indexRawSize * 4;
    if (estimateOut > kMaxIndexSize) estimateOut = kMaxIndexSize;
    auto decompIndex = ZlibDecompress(raw.data() + indexOff, indexRawSize, estimateOut);
    const uint8_t* idxData = nullptr;
    size_t idxSize = 0;
    if (!decompIndex.empty()) {
        idxData = decompIndex.data();
        idxSize = decompIndex.size();
    } else {
        // Might be raw (uncompressed) index
        idxData = raw.data() + indexOff;
        idxSize = indexRawSize;
    }

    auto files = ParseIndex(idxData, idxSize);
    int total = (int)files.size();

    fs::create_directories(outputDir);

    for (int i = 0; i < total; ++i) {
        auto& fe = files[i];
        // Build output path from UTF-16LE name
        std::string relPath(fe.name.begin(), fe.name.end());
        std::replace(relPath.begin(), relPath.end(), '\\', '/');

        // C2: path traversal guard ?? reject ".." components
        if (relPath.find("..") != std::string::npos) {
            fprintf(stderr, "[XP3] Rejected path traversal: %s\n", relPath.c_str());
            continue;
        }

        fs::path outAbs = fs::absolute(outputDir).lexically_normal();
        fs::path filePath = (outAbs / relPath).lexically_normal();
        // Verify filePath prefix is within outputDir
        std::string outAbsStr = outAbs.string();
        std::string filePathStr = filePath.string();
        if (filePathStr.size() < outAbsStr.size() ||
            filePathStr.compare(0, outAbsStr.size(), outAbsStr) != 0) {
            fprintf(stderr, "[XP3] Rejected path escape: %s\n", relPath.c_str());
            continue;
        }
        std::string outPath = filePathStr;

        // Create parent directories
        fs::path p(outPath);
        if (p.has_parent_path())
            fs::create_directories(p.parent_path());

        // Extract segments
        std::vector<uint8_t> outData;
        for (auto& seg : fe.segments) {
            if (seg.offset + seg.arcSize > raw.size()) continue;
            const uint8_t* segData = raw.data() + seg.offset;
            bool compressed = (seg.flags & XP3_SEGM_MASK) == XP3_ENC_ZLIB;

            if (compressed) {
                auto d = ZlibDecompress(segData, (size_t)seg.arcSize, (size_t)seg.orgSize);
                if (!d.empty())
                    outData.insert(outData.end(), d.begin(), d.end());
            } else {
                outData.insert(outData.end(), segData, segData + (size_t)seg.orgSize);
            }
        }

        WriteFileBytes(outPath, outData.data(), outData.size());
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
    size_t indexRawSize = raw.size() - (size_t)indexOff;
    if (indexOff >= raw.size() || indexRawSize == 0) return {};

    // M1: prevent decompression bomb (cap at 256 MB) ¡ª same as unpack()
    constexpr size_t kMaxIndexSize = 256 * 1024 * 1024;
    if (indexRawSize > kMaxIndexSize) return {};
    size_t estimateOut = indexRawSize * 4;
    if (estimateOut > kMaxIndexSize) estimateOut = kMaxIndexSize;
    auto decompIndex = ZlibDecompress(raw.data() + indexOff, indexRawSize, estimateOut);
    const uint8_t* idxData = decompIndex.empty() ? raw.data() + indexOff : decompIndex.data();
    size_t idxSize = decompIndex.empty() ? indexRawSize : decompIndex.size();

    return ParseIndex(idxData, idxSize);
}

} // namespace Caesura