// CARCWriter -- Create and write CARC archives.
#pragma once
#include "CARCFormat.h"
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

namespace caesura::carc {

struct PendingFile {
    std::string relativePath;
    uint8_t     pathHash[PATH_HASH_SIZE];
    std::vector<uint8_t> data;
    uint64_t    originalSize;
};

class CARCWriter {
public:
    CARCWriter() = default;
    ~CARCWriter() { if (m_output.is_open()) m_output.close(); }

    // Begin writing a CARC file. keyPath is the path to write the public key.
    // If privateKeyPath is non-empty, also generate and save keypair.
    bool create(const std::string& outputPath,
                const std::string& privateKeyPath = "",
                const std::string& publicKeyPath = "");

    // Add a file to the archive. Data is copied.
    bool addFile(const std::string& relativePath,
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

} // namespace caesura::carc