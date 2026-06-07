// CRLManager -- Certificate Revocation List management for CARC chain trust.
// Spec [10.2.63]: Offline/Online/Hybrid modes, set-based revocation lookup,
// JSON CRL format with version, timestamp, revoked fingerprints, signature.
#pragma once
#include <string>
#include <unordered_set>
#include <vector>
#include <cstdint>

namespace caesura::carc {

/// Operating mode for CRL checking
enum class CRLMode {
    Offline,  ///< Use only locally cached CRL
    Online,   ///< Fetch CRL from remote URL
    Hybrid    ///< Try online first, fallback to local cache on timeout
};

/// Certificate Revocation List Manager
/// Maintains a set of revoked certificate fingerprints and supports
/// online CRL fetching with local caching and signature verification.
class CRLManager {
public:
    CRLManager() = default;
    ~CRLManager() = default;

    // ── Configuration ──────────────────────────────────────────────────

    /// Set operating mode (default: Hybrid)
    void setMode(CRLMode mode) { m_mode = mode; }
    CRLMode mode() const { return m_mode; }

    /// Set the CRL fetch URL
    void setOnlineURL(const std::string& url) { m_onlineURL = url; }
    const std::string& onlineURL() const { return m_onlineURL; }

    /// Set local cache path (e.g. "saves/crl_cache.json")
    void setCachePath(const std::string& path) { m_cachePath = path; }

    /// Set the root Ed25519 public key for CRL signature verification
    void setRootPublicKey(const uint8_t key[32]);

    // ── CRL Operations ─────────────────────────────────────────────────

    /// Verify whether a certificate (by fingerprint) is revoked.
    /// Returns true if the cert is NOT revoked (i.e., passes CRL check).
    /// Uses the configured mode to determine lookup strategy.
    bool verify(const std::string& pubKeyFingerprint, CRLMode mode);

    /// Fetch CRL from configured online URL, verify signature, cache locally.
    /// Returns true on successful fetch + verification.
    bool fetchOnline(const std::string& url);

    /// Load CRL from local cache file.
    /// Returns true if cache exists and passes verification.
    bool loadLocalCache();

    /// Check if a specific fingerprint is in the current revoked set.
    bool isRevoked(const std::string& fingerprint) const;

    /// Add a fingerprint to the revoked set (for testing / local updates).
    void addRevoked(const std::string& fingerprint);

    /// Return the number of revoked fingerprints currently loaded.
    size_t revokedCount() const { return m_revoked.size(); }

    /// Clear the in-memory revoked set.
    void clear();

    // ── Serialization ──────────────────────────────────────────────────

    /// Parse CRL JSON and populate revoked set.
    /// Expected format: { "version":1, "timestamp":..., "revoked_fingerprints":["hex...",...], "signature":"hex..." }
    /// Returns true if signature verification passes.
    bool parseCRL(const std::string& json);

    /// Generate CRL JSON from current revoked set.
    std::string generateCRL() const;

    // ── Utility ────────────────────────────────────────────────────────

    /// Compute SHA-256 fingerprint of binary public key data as hex string
    static std::string computeFingerprint(const uint8_t* data, size_t len);

private:
    CRLMode m_mode = CRLMode::Hybrid;
    std::string m_onlineURL;
    std::string m_cachePath = "saves/crl_cache.json";

    uint8_t m_rootPublicKey[32] = {};
    bool m_hasRootKey = false;

    std::unordered_set<std::string> m_revoked;  ///< SHA-256 hex fingerprints of revoked certs

    uint64_t m_crlTimestamp = 0;  ///< Timestamp from last loaded/parsed CRL
    uint32_t m_crlVersion = 0;    ///< Version from last loaded/parsed CRL
};

} // namespace caesura::carc
