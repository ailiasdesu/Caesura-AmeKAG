// CARCWriter -- Create and write CARC archives.
#pragma once
#include "CARCFormat.h"
#include "api/IArchiveWriter.h"
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

namespace Caesura::carc {

struct PendingFile {
    std::string relativePath;
    uint8_t     pathHash[PATH_HASH_SIZE];
    std::vector<uint8_t> data;
    uint64_t    originalSize;
};

class CARCWriter : public IArchiveWriter {
public:
    CARCWriter() = default;
    ~CARCWriter() { if (m_output.is_open()) m_output.close(); }

    // Begin writing a CARC file. keyPath is the path to write the public key.
    // If privateKeyPath is non-empty, also generate and save keypair.
    bool create(const std::string& outputPath,
                const std::string& privateKeyPath = "",
                const std::string& publicKeyPath = "");

    // Add a file to the archive. Data is copied. Idempotent by relative path:
    // adding the same path again updates the existing pending entry (last write
    // wins) instead of appending a duplicate -- the index holds one entry per
    // unique relative path, matching numFiles() and the reader's hash-keyed map.
    bool addFile(const std::string& relativePath,
                 const uint8_t* data, size_t size);

    // Add a file by its precomputed path hash (used by DeltaCARC apply,
    // where plaintext paths are not recoverable from CARC indexes). Also
    // idempotent by path hash, with the same "update existing" semantics.
    bool addFileByHash(const uint8_t pathHash[PATH_HASH_SIZE],
                       const uint8_t* data, size_t size);

    // Finalize: encrypt index, write header, sign, write signature + public key.
    bool finalize();

private:
    std::ofstream m_output;
    std::string   m_outputPath;
    std::string   m_publicKeyPath;

    uint8_t m_publicKey[PUBLICKEY_SIZE] = {};
    uint8_t m_privateKey[64] = {};

    std::vector<PendingFile> m_pendingFiles;
    std::vector<FileEntry>   m_entries;

    // Accumulated content block (encrypted per-file data)
    std::vector<uint8_t> m_contentBlock;
};

} // namespace Caesura::carc