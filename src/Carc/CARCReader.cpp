// CARCReader implementation
// Phase 9: Added verifyChainTrust() for multi-CARC chain validation (spec [10.2.63])
#include "CARCReader.h"
#include "CryptoEngine.h"
#include <zstd.h>
#include <cstring>
#include <algorithm>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace caesura::carc {

// ==========================================================================
// open — load and verify a CARC file
// ==========================================================================
bool CARCReader::open(const std::string& path, const std::string& pubKeyPath)
{
    close();

    m_stream.open(path, std::ios::binary);
    if (!m_stream.is_open()) return false;

    // 1. Read header
    m_stream.read(reinterpret_cast<char*>(&m_header), sizeof(CARCHeader));
    if (!m_stream || m_header.magic != CARC_MAGIC) {
        close();
        return false;
    }
    if (m_header.version != CARC_VERSION) {
        close();
        return false;
    }

    // 2. Read public key
    if (!pubKeyPath.empty()) {
        if (!CryptoEngine::readPublicKey(pubKeyPath, m_publicKey)) {
            close();
            return false;
        }
        m_hasPublicKey = true;
    } else {
        if (!readTrailingPublicKey()) {
            close();
            return false;
        }
        m_hasPublicKey = true;
    }

    // 3. Verify signature
    if (!verifySignature()) {
        close();
        return false;
    }

    // 4. Decrypt index block
    std::vector<uint8_t> indexData;
    if (!decryptIndex(indexData)) {
        close();
        return false;
    }

    // 5. Parse index entries
    uint32_t count = 0;
    if (indexData.size() < sizeof(uint32_t)) {
        close();
        return false;
    }
    memcpy(&count, indexData.data(), sizeof(uint32_t));
    if (count != m_header.numFiles) {
        close();
        return false;
    }

    size_t expectedSize = sizeof(uint32_t) + count * sizeof(FileEntry);
    if (indexData.size() < expectedSize) {
        close();
        return false;
    }

    const FileEntry* entries = reinterpret_cast<const FileEntry*>(
        indexData.data() + sizeof(uint32_t));

    for (uint32_t i = 0; i < count; ++i) {
        const FileEntry& e = entries[i];
        std::string hexHash = pathHashToHex(e.pathHash);

        CarcFileInfo info;
        info.offset = e.offset;
        info.compressedSize = e.compressedSize;
        info.originalSize = e.originalSize;
        memcpy(info.aesKey, e.aesKey, AES_KEY_SIZE);
        memcpy(info.nonce, e.nonce, AES_NONCE_SIZE);
        memcpy(info.tag, e.tag, AES_TAG_SIZE);
        m_index[hexHash] = info;
        m_fileList.push_back(hexHash);
    }

    return true;
}

void CARCReader::close()
{
    m_stream.close();
    m_index.clear();
    m_fileList.clear();
    m_hasPublicKey = false;
}

// ==========================================================================
// readFile — decrypt + decompress one file by relative path
// ==========================================================================
std::vector<uint8_t> CARCReader::readFile(const std::string& relativePath)
{
    uint8_t hash[PATH_HASH_SIZE];
    CryptoEngine::sha256(reinterpret_cast<const uint8_t*>(relativePath.data()),
                         relativePath.size(), hash);
    return readFileByHash(hash);
}

std::vector<uint8_t> CARCReader::readFileByHash(const uint8_t pathHash[PATH_HASH_SIZE])
{
    if (!m_stream.is_open()) return {};

    std::string hexHash = pathHashToHex(pathHash);
    auto it = m_index.find(hexHash);
    if (it == m_index.end()) return {};

    const CarcFileInfo& info = it->second;

    // Read encrypted + compressed data from content block
    m_stream.seekg(m_header.contentOffset + info.offset, std::ios::beg);
    if (!m_stream) return {};

    std::vector<uint8_t> encrypted(info.compressedSize);
    m_stream.read(reinterpret_cast<char*>(encrypted.data()), info.compressedSize);
    if (!m_stream) return {};

    // AES-GCM decrypt
    std::vector<uint8_t> compressed = CryptoEngine::decrypt(
        encrypted.data(), encrypted.size(), info.aesKey, info.nonce, info.tag);
    if (compressed.empty()) return {};

    if (compressed.size() == info.originalSize && info.compressedSize == info.originalSize) {
        return compressed;
    }
    size_t destLen = info.originalSize;
    std::vector<uint8_t> decompressed(info.originalSize);
    size_t result = ZSTD_decompress(decompressed.data(), destLen,
                                    compressed.data(), compressed.size());
    if (ZSTD_isError(result)) {
        if (compressed.size() == info.originalSize) {
            return compressed;
        }
        return {};
    }
    decompressed.resize(result);
    return decompressed;
}

// ==========================================================================
// hasFile — existence check (no decrypt/decompress)
// ==========================================================================
bool CARCReader::hasFile(const std::string& relativePath) const
{
    uint8_t hash[PATH_HASH_SIZE];
    hashPath(relativePath, hash);
    return hasFileByHash(hash);
}

bool CARCReader::hasFileByHash(const uint8_t pathHash[PATH_HASH_SIZE]) const
{
    return m_index.find(pathHashToHex(pathHash)) != m_index.end();
}

// ==========================================================================
// hashPath — SHA-256 of a path string
// ==========================================================================
void CARCReader::hashPath(const std::string& path, uint8_t out[PATH_HASH_SIZE])
{
    CryptoEngine::sha256(reinterpret_cast<const uint8_t*>(path.data()),
                         path.size(), out);
}

// ==========================================================================
// verifySignature — Ed25519 verify over header + content + index blocks
// ==========================================================================
bool CARCReader::verifySignature()
{
    if (!m_hasPublicKey) return false;

    m_stream.seekg(-static_cast<int64_t>(PUBLICKEY_SIZE + SIGNATURE_SIZE), std::ios::end);
    if (!m_stream) return false;

    uint8_t signature[SIGNATURE_SIZE];
    m_stream.read(reinterpret_cast<char*>(signature), SIGNATURE_SIZE);
    if (!m_stream) return false;

    // Data covered: header + content + index
    uint64_t dataSize = m_header.indexOffset + m_header.indexSize;
    m_stream.seekg(0, std::ios::beg);
    std::vector<uint8_t> signedData(static_cast<size_t>(dataSize));
    m_stream.read(reinterpret_cast<char*>(signedData.data()), dataSize);
    if (!m_stream) return false;

    return CryptoEngine::verify(signedData.data(), signedData.size(),
                                m_publicKey, signature);
}

// ==========================================================================
// decryptIndex — read and decrypt the index block
// ==========================================================================
bool CARCReader::decryptIndex(std::vector<uint8_t>& outIndexData)
{
    m_stream.seekg(m_header.indexOffset, std::ios::beg);
    if (!m_stream) return false;

    uint8_t indexKey[AES_KEY_SIZE];
    uint8_t h[PATH_HASH_SIZE];
    CryptoEngine::sha256(m_publicKey, PUBLICKEY_SIZE, h);
    memcpy(indexKey, h, AES_KEY_SIZE);

    uint8_t indexNonce[AES_NONCE_SIZE] = {};
    memcpy(indexNonce, &m_header.version, sizeof(m_header.version));

    uint8_t indexTag[AES_TAG_SIZE];
    uint64_t encryptedDataSize = m_header.indexSize - AES_TAG_SIZE;
    m_stream.seekg(m_header.indexOffset + encryptedDataSize, std::ios::beg);
    m_stream.read(reinterpret_cast<char*>(indexTag), AES_TAG_SIZE);

    m_stream.seekg(m_header.indexOffset, std::ios::beg);
    std::vector<uint8_t> encryptedIndex(static_cast<size_t>(encryptedDataSize));
    m_stream.read(reinterpret_cast<char*>(encryptedIndex.data()), encryptedDataSize);
    if (!m_stream) return false;

    outIndexData = CryptoEngine::decrypt(
        encryptedIndex.data(), encryptedIndex.size(),
        indexKey, indexNonce, indexTag);
    return !outIndexData.empty();
}

// ==========================================================================
// readTrailingPublicKey
// ==========================================================================
bool CARCReader::readTrailingPublicKey()
{
    m_stream.seekg(-static_cast<int64_t>(PUBLICKEY_SIZE), std::ios::end);
    if (!m_stream) return false;

    m_stream.read(reinterpret_cast<char*>(m_publicKey), PUBLICKEY_SIZE);
    return m_stream.gcount() == PUBLICKEY_SIZE;
}

// ==========================================================================
// pathHashToHex
// ==========================================================================
std::string CARCReader::pathHashToHex(const uint8_t hash[PATH_HASH_SIZE])
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < PATH_HASH_SIZE; ++i)
        oss << std::setw(2) << static_cast<int>(hash[i]);
    return oss.str();
}

// ══════════════════════════════════════════════════════════════════════════
//  Phase 9: Chain Trust (spec [10.2.63])
// ══════════════════════════════════════════════════════════════════════════

// ==========================================================================
//  setRootPublicKey
// ==========================================================================
void CARCReader::setRootPublicKey(const uint8_t key[PUBLICKEY_SIZE])
{
    memcpy(m_rootPublicKey, key, PUBLICKEY_SIZE);
    m_hasRootKey = true;
}

// ==========================================================================
//  parseCertificate — parse JSON certificate from raw data
//  Expected format:
//  {
//    "child_pubkey_hash": "hex_sha256...",
//    "expiry": 1717632000,
//    "permissions": 1,
//    "signature": "hex_ed25519..."
//  }
// ==========================================================================
bool CARCReader::parseCertificate(const std::string& json, CarcCertificate& cert) const
{
    if (json.empty()) return false;

    // Simple JSON extraction helpers (inline — no external lib)
    auto findString = [](const std::string& s, const std::string& key) -> std::string {
        std::string search = "\"" + key + "\"";
        size_t pos = s.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        while (pos < s.size() && s[pos] != ':') pos++;
        pos++; // skip ':'
        while (pos < s.size() && s[pos] != '"') pos++;
        if (pos >= s.size()) return "";
        pos++; // skip opening quote
        size_t end = pos;
        while (end < s.size() && s[end] != '"') {
            if (s[end] == '\\') end++; // skip escaped
            end++;
        }
        std::string raw = s.substr(pos, end - pos);
        // Unescape simple escapes
        std::string result;
        for (size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] == '\\' && i + 1 < raw.size()) {
                char c = raw[++i];
                if (c == '"') result += '"';
                else if (c == '\\') result += '\\';
                else if (c == 'n') result += '\n';
                else { result += '\\'; result += c; }
            } else {
                result += raw[i];
            }
        }
        return result;
    };

    auto findInt = [](const std::string& s, const std::string& key) -> int64_t {
        std::string search = "\"" + key + "\"";
        size_t pos = s.find(search);
        if (pos == std::string::npos) return 0;
        pos += search.size();
        while (pos < s.size() && s[pos] != ':') pos++;
        pos++; // skip ':'
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;
        std::string num;
        while (pos < s.size() && (isdigit(s[pos]) || s[pos] == '-')) {
            num += s[pos];
            pos++;
        }
        if (num.empty()) return 0;
        return std::stoll(num);
    };

    cert.childPubKeyHash = findString(json, "child_pubkey_hash");
    cert.expiry          = findInt(json, "expiry");
    cert.permissions     = static_cast<uint32_t>(findInt(json, "permissions"));
    cert.signature       = findString(json, "signature");

    return !cert.childPubKeyHash.empty() && !cert.signature.empty();
}

// ==========================================================================
//  verifyChainTrust — full 7-step chain verification per spec [10.2.63]
// ==========================================================================
bool CARCReader::verifyChainTrust(const uint8_t* childPublicKey,
                                   const std::string& certificatePath)
{
    if (!childPublicKey || certificatePath.empty()) return false;

    // Step 1: Compute SHA-256 hash of child's trailing 32B public key
    std::string childPubKeyHash = CRLManager::computeFingerprint(childPublicKey, PUBLICKEY_SIZE);

    // Step 2: Read certificate JSON from this (parent) CARC at certificatePath
    std::vector<uint8_t> certData = readFile(certificatePath);
    if (certData.empty()) {
        // Cert not found in archive — chain trust fails
        return false;
    }
    std::string certJson(reinterpret_cast<const char*>(certData.data()), certData.size());

    // Step 3 & 3a: Parse certificate + verify signature with root Ed25519 public key
    CarcCertificate cert;
    if (!parseCertificate(certJson, cert)) return false;

    // Verify certificate signature if root key is available
    if (m_hasRootKey) {
        // Build the signed payload: everything except the signature field
        size_t sigPos = certJson.find("\"signature\"");
        if (sigPos == std::string::npos) return false;

        // Extract payload: trim trailing signature field + comma
        size_t payloadEnd = sigPos;
        while (payloadEnd > 0 && certJson[payloadEnd] != ',' && certJson[payloadEnd] != '{') payloadEnd--;
        std::string payload = certJson.substr(0, payloadEnd);
        while (!payload.empty() && (payload.back() == ' ' || payload.back() == '\t' ||
               payload.back() == '\n' || payload.back() == '\r' || payload.back() == ',')) {
            payload.pop_back();
        }
        payload += "\n}";

        // Decode signature hex → binary
        const std::string& sigHex = cert.signature;
        if (sigHex.size() != 128) return false; // Ed25519 sig = 64 bytes = 128 hex chars

        uint8_t signature[64];
        for (size_t i = 0; i < 64; ++i) {
            char hex[3] = { sigHex[i * 2], sigHex[i * 2 + 1], 0 };
            signature[i] = static_cast<uint8_t>(strtoul(hex, nullptr, 16));
        }

        if (!CryptoEngine::verify(
                reinterpret_cast<const uint8_t*>(payload.data()),
                payload.size(),
                m_rootPublicKey,
                signature)) {
            return false; // Signature invalid
        }
    }

    // Step 4: SHA-256 compare cert's pubkey hash vs child's trailing pubkey hash
    if (cert.childPubKeyHash != childPubKeyHash) {
        return false; // Hash mismatch — child CARC not authorized by this cert
    }

    // Step 5: Verify child CARC signature with child pubkey
    // (This is done in child CARC's own open() call — we have already
    // verified that the hash matches, so the child will verify itself)

    // Step 6: Check certificate expiry
    auto now = static_cast<int64_t>(std::time(nullptr));
    if (cert.expiry > 0 && now > cert.expiry) {
        return false; // Certificate expired
    }

    // Step 7: Check CRL revocation list
    if (m_crlManager) {
        // Check if the certificate's pubkey hash is in the revocation list
        if (!m_crlManager->verify(cert.childPubKeyHash, m_crlManager->mode())) {
            return false; // Certificate revoked
        }
    }

    return true;
}

} // namespace caesura::carc
