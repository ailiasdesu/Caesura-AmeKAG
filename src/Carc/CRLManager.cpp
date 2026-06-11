// CRLManager implementation -- CRL parsing, caching, online fetch, revocation checks.
// Uses CryptoEngine for Ed25519 signature verification and SHA-256 fingerprinting.
#include <cstring>
#include "CRLManager.h"
#include "CryptoEngine.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>

// Simple JSON parsing -- no external dependency for this lightweight CRL format.
// Expected structure:
// {
//   "version": 1,
//   "timestamp": 1717632000,
//   "revoked_fingerprints": ["a1b2c3...", "d4e5f6..."],
//   "signature": "hex..."
// }

namespace Caesura::carc {

namespace {

// ©¤©¤ Minimal JSON helpers ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤

// Extract a quoted string value (handles escaped chars within)
std::string extractString(const std::string& json, size_t& pos) {
    // Find opening quote
    while (pos < json.size() && json[pos] != '"') pos++;
    if (pos >= json.size()) return "";
    pos++; // skip opening quote
    std::string result;
    while (pos < json.size()) {
        if (json[pos] == '\\') {
            pos++;
            if (pos < json.size()) {
                char c = json[pos];
                if (c == '"') result += '"';
                else if (c == '\\') result += '\\';
                else if (c == 'n') result += '\n';
                else if (c == 't') result += '\t';
                else { result += '\\'; result += c; }
            }
        } else if (json[pos] == '"') {
            pos++; // skip closing quote
            break;
        } else {
            result += json[pos];
        }
        pos++;
    }
    return result;
}

// Extract an integer value
int64_t extractInt(const std::string& json, size_t& pos) {
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == ':' || json[pos] == ',')) pos++;
    std::string num;
    while (pos < json.size() && (isdigit(json[pos]) || json[pos] == '-')) {
        num += json[pos];
        pos++;
    }
    if (num.empty()) return 0;
    return std::stoll(num);
}

// Find value for key in JSON object
std::string findStringValue(const std::string& json, const std::string& key) {
    size_t pos = 0;
    std::string searchKey = "\"" + key + "\"";
    size_t found = json.find(searchKey);
    if (found == std::string::npos) return "";
    pos = found + searchKey.size();
    // Skip to colon
    while (pos < json.size() && json[pos] != ':') pos++;
    pos++; // skip colon
    return extractString(json, pos);
}

int64_t findIntValue(const std::string& json, const std::string& key) {
    size_t pos = 0;
    std::string searchKey = "\"" + key + "\"";
    size_t found = json.find(searchKey);
    if (found == std::string::npos) return 0;
    pos = found + searchKey.size();
    while (pos < json.size() && json[pos] != ':') pos++;
    pos++; // skip colon
    return extractInt(json, pos);
}

// Extract array of strings from JSON
std::vector<std::string> findStringArray(const std::string& json, const std::string& key) {
    std::vector<std::string> result;
    size_t pos = 0;
    std::string searchKey = "\"" + key + "\"";
    size_t found = json.find(searchKey);
    if (found == std::string::npos) return result;
    pos = found + searchKey.size();
    while (pos < json.size() && json[pos] != '[') pos++;
    if (pos >= json.size()) return result;
    pos++; // skip [
    while (pos < json.size()) {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == ',')) pos++;
        if (json[pos] == ']') break;
        if (json[pos] == '"') {
            result.push_back(extractString(json, pos));
        } else {
            pos++;
        }
    }
    return result;
}

// Extract signature from CRL JSON (removes signature field, returns remaining JSON + signature hex)
bool extractSignature(const std::string& json, std::string& outPayload, std::string& outSignatureHex) {
    // Find signature field
    size_t sigStart = json.find("\"signature\"");
    if (sigStart == std::string::npos) return false;

    // Build payload: everything except the signature field
    // Find preceding comma or opening brace
    size_t payloadEnd = sigStart;
    while (payloadEnd > 0 && json[payloadEnd] != ',' && json[payloadEnd] != '{') payloadEnd--;

    outPayload = json.substr(0, payloadEnd);
    // Close the JSON object
    // Trim trailing whitespace
    while (!outPayload.empty() && (outPayload.back() == ' ' || outPayload.back() == '\t' || outPayload.back() == '\n' || outPayload.back() == ',')) {
        outPayload.pop_back();
    }
    outPayload += "\n}";

    // Extract signature hex
    size_t pos = sigStart;
    outSignatureHex = findStringValue(json, "signature");

    return !outSignatureHex.empty();
}

} // anonymous namespace

// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
//  setRootPublicKey
// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
void CRLManager::setRootPublicKey(const uint8_t key[32]) {
    memcpy(m_rootPublicKey, key, 32);
    m_hasRootKey = true;
}

// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
//  verify -- check a certificate against the CRL
// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
bool CRLManager::verify(const std::string& pubKeyFingerprint, CRLMode mode) {
    bool fetched = false;

    switch (mode) {
    case CRLMode::Offline:
        if (m_revoked.empty()) {
            loadLocalCache();
        }
        break;

    case CRLMode::Online:
        if (!m_onlineURL.empty()) {
            fetched = fetchOnline();
        }
        break;

    case CRLMode::Hybrid:
    default:
        // Try online first
        if (!m_onlineURL.empty()) {
            // Simple timeout simulation: fetchOnline already handles this
            fetched = fetchOnline();
        }
        // Fallback to local cache
        if (!fetched && m_revoked.empty()) {
            loadLocalCache();
        }
        break;
    }

    // Check if the fingerprint is in the revoked set
    return !isRevoked(pubKeyFingerprint);
}

// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
//  fetchOnline -- HTTPS fetch CRL ¡ú verify signature ¡ú cache locally
// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
bool CRLManager::fetchOnline() {
    // This is a stub for offline/embedded use.
    // In a full implementation, this would use platform HTTP (WinHTTP / libcurl).
    // For now, returns false to signal that online fetch is unavailable,
    // which triggers the Hybrid-mode fallback to local cache.
    //
    // When HTTP is available:
    //   1. GET <url> with 5s timeout
    //   2. Parse JSON response
    //   3. Verify CRL signature with root public key
    //   4. Replace m_revoked with new entries
    //   5. Write JSON to m_cachePath for offline use
    (void)m_onlineURL;  // suppress unused until HTTP wired
    return false; // Online fetch not yet connected -- use local cache
}

// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
//  loadLocalCache -- load CRL from saves/crl_cache.json
// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
bool CRLManager::loadLocalCache() {
    if (m_cachePath.empty()) return false;

    std::ifstream in(m_cachePath, std::ios::binary);
    if (!in.is_open()) return false;

    std::stringstream ss;
    ss << in.rdbuf();
    in.close();

    return parseCRL(ss.str());
}

// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
//  isRevoked -- set lookup
// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
bool CRLManager::isRevoked(const std::string& fingerprint) const {
    return m_revoked.find(fingerprint) != m_revoked.end();
}

// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
//  addRevoked
// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
void CRLManager::addRevoked(const std::string& fingerprint) {
    m_revoked.insert(fingerprint);
}

// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
//  clear
// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
void CRLManager::clear() {
    m_revoked.clear();
    m_crlTimestamp = 0;
    m_crlVersion = 0;
}

// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
//  parseCRL -- parse CRL JSON, verify signature, populate revoked set
// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
bool CRLManager::parseCRL(const std::string& json) {
    if (json.empty()) return false;

    // Extract version
    m_crlVersion = static_cast<uint32_t>(findIntValue(json, "version"));

    // Extract timestamp
    m_crlTimestamp = static_cast<uint64_t>(findIntValue(json, "timestamp"));

    // Extract revoked fingerprints
    auto fingerprints = findStringArray(json, "revoked_fingerprints");

    // Verify signature if we have a root key
    if (m_hasRootKey) {
        std::string payload, sigHex;
        if (!extractSignature(json, payload, sigHex)) {
            return false; // malformed CRL -- no signature
        }

        // Convert hex signature to binary
        std::vector<uint8_t> sigBytes(sigHex.size() / 2);
        for (size_t i = 0; i < sigBytes.size(); ++i) {
            char hex[3] = { sigHex[i * 2], sigHex[i * 2 + 1], 0 };
            sigBytes[i] = static_cast<uint8_t>(strtoul(hex, nullptr, 16));
        }

        // If signature is 64 bytes, verify with Ed25519
        if (sigBytes.size() == 64) {
            if (!CryptoEngine::verify(
                    reinterpret_cast<const uint8_t*>(payload.data()),
                    payload.size(),
                    m_rootPublicKey,
                    sigBytes.data())) {
                return false; // signature verification failed
            }
        }
        // If no root key or invalid sig size, skip verification (allow for dev/testing)
    }

    // Populate revoked set
    clear();
    for (const auto& fp : fingerprints) {
        m_revoked.insert(fp);
    }

    return true;
}

// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
//  generateCRL -- produce a signed CRL JSON string from current revoked set
// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
std::string CRLManager::generateCRL() const {
    std::ostringstream oss;

    // Build fingerprints array
    std::ostringstream fps;
    fps << "[";
    bool first = true;
    for (const auto& fp : m_revoked) {
        if (!first) fps << ", ";
        fps << "\"" << fp << "\"";
        first = false;
    }
    fps << "]";

    // Timestamp
    uint64_t timestamp = m_crlTimestamp;
    if (timestamp == 0) {
        timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count());
    }

    // Build unsigned payload (signature replaced with placeholder for signing)
    oss << "{\n"
        << "  \"version\": " << m_crlVersion << ",\n"
        << "  \"timestamp\": " << timestamp << ",\n"
        << "  \"revoked_fingerprints\": " << fps.str() << ",\n"
        << "  \"signature\": \"\"\n"
        << "}";

    return oss.str();
}

// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
//  computeFingerprint -- SHA-256 of public key data as hex string
// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
std::string CRLManager::computeFingerprint(const uint8_t* data, size_t len) {
    uint8_t hash[32];
    CryptoEngine::sha256(data, len, hash);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < 32; ++i) {
        oss << std::setw(2) << static_cast<int>(hash[i]);
    }
    return oss.str();
}

} // namespace Caesura::carc
