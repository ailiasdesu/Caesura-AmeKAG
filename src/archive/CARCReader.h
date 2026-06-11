// CARCReader -- Read, verify, decrypt, and decompress files from a CARC archive.
// Phase 9: Chain trust verification (spec [10.2.63]) via verifyChainTrust().
#pragma once
#include "CARCFormat.h"
#include "CRLManager.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <memory>
#include <mutex>

namespace Caesura::carc {

struct CarcFileInfo {
    uint64_t offset;
    uint64_t compressedSize;
    uint64_t originalSize;
    uint8_t  aesKey[AES_KEY_SIZE];
    uint8_t  nonce[AES_NONCE_SIZE];
    uint8_t  tag[AES_TAG_SIZE];
};

/// Certificate metadata for chain trust verification
struct CarcCertificate {
    std::string childPubKeyHash;   ///< SHA-256 hex of the child CARC''s public key
    int64_t     expiry = 0;         ///< Unix timestamp when certificate expires
    uint32_t    permissions = 0;    ///< Permission flags (reserved)
    std::string signature;          ///< Ed25519 signature (hex) from root key
};

class CARCReader {
    friend class CarcAssetProvider;
public:
    CARCReader() = default;
    ~CARCReader() { close(); }

    // Open a .carc file.  pubKeyPath can be empty; the reader will
    // look for a .pub key alongside the archive or use an embedded key.
    bool open(const std::string& path, const std::string& pubKeyPath = "");

    void close();

    // Read a single file: decrypt + decompress.
    // Returns empty vector if the file is not found or on error.
    std::vector<uint8_t> readFile(const std::string& relativePath);
    std::vector<uint8_t> readFileByHash(const uint8_t pathHash[PATH_HASH_SIZE]);

    // Check if a file exists in the index (does not decrypt/decompress).
    bool hasFile(const std::string& relativePath) const;
    bool hasFileByHash(const uint8_t pathHash[PATH_HASH_SIZE]) const;

    // Metadata
    const std::vector<std::string>& fileList() const { return m_fileList; }
    const std::unordered_map<std::string, CarcFileInfo>& index() const { return m_index; }
    size_t numFiles() const { return m_fileList.size(); }
    uint32_t version() const { return m_header.version; }
    bool isOpen() const { return m_stream.is_open(); }

    // Utility: hash a path string to 32-byte SHA-256
    static void hashPath(const std::string& path, uint8_t out[PATH_HASH_SIZE]);

    // ©¤©¤ Phase 9: Chain Trust (spec [10.2.63]) ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤

    /// Verify chain trust: a child CARC is authorized by a certificate
    /// stored in this (parent) CARC. The certificate is a JSON blob at
    /// certificatePath within this archive.
    /// Steps:
    ///   1. Read child CARC''s trailing 32B public key
    ///   2. Read certificate JSON from this CARC at certificatePath
    ///   3. Verify certificate signature with root Ed25519 public key
    ///   4. SHA-256 compare cert''s pubkey hash vs child''s trailing pubkey
    ///   5. Verify child CARC signature with child pubkey (done in child.open)
    ///   6. Check certificate expiry
    ///   7. Check CRL revocation list
    bool verifyChainTrust(const uint8_t* childPublicKey,
                          const std::string& certificatePath);

    /// Set the CRL manager for revocation checks
    void setCRLManager(CRLManager* crl) { m_crlManager = crl; }

    /// Set the root Ed25519 public key for certificate verification
    void setRootPublicKey(const uint8_t key[PUBLICKEY_SIZE]);

    /// Get the public key read from this CARC
    const uint8_t* publicKey() const { return m_publicKey; }
    bool hasPublicKey() const { return m_hasPublicKey; }

public:
    CARCHeader m_header;
    // pathHash (32-byte as hex string) ¡ú FileInfo
    std::unordered_map<std::string, CarcFileInfo> m_index;
    std::vector<std::string> m_fileList;
    std::ifstream m_stream;
    std::mutex    m_streamMutex;  // serialize concurrent seekg/read from JobSystem workers

    uint8_t m_publicKey[PUBLICKEY_SIZE];
    bool m_hasPublicKey = false;

    bool verifySignature();
    bool decryptIndex(std::vector<uint8_t>& outIndexData);

    // Read public key from end of file (format: [data...][64B signature][32B pubkey])
    bool readTrailingPublicKey();

    static std::string pathHashToHex(const uint8_t hash[PATH_HASH_SIZE]);

    // ©¤©¤ Chain trust ©¤©¤
    CRLManager* m_crlManager = nullptr;
    uint8_t m_rootPublicKey[PUBLICKEY_SIZE] = {};
    bool m_hasRootKey = false;

    /// Parse a certificate JSON from raw data
    bool parseCertificate(const std::string& json, CarcCertificate& cert) const;
};

} // namespace Caesura::carc
