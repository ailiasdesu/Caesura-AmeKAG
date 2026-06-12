// CryptoEngine -- AES-256-GCM + SHA-256. Win: BCrypt; macOS/Linux: OpenSSL EVP.
// Ed25519 via orlp (cross-platform). File I/O via std::ifstream (cross-platform).
// Implements ICryptoEngine (pointer+length API) while keeping static wrappers
// for backward compatibility with existing internal archive code.
#pragma once
#include "CARCFormat.h"
#include "api/ICryptoEngine.h"
#include <vector>
#include <string>
#include <cstdint>

namespace Caesura::carc {

class CryptoEngine : public ICryptoEngine {
public:
    static CryptoEngine& instance();

    // --- ICryptoEngine interface (pointer+length) ---
    std::vector<uint8_t> encrypt(
        const uint8_t* plaintext, size_t plaintextLen,
        const uint8_t* key, size_t keyLen,
        uint8_t* nonce, size_t nonceLen,
        uint8_t* tag, size_t tagLen) override;

    std::vector<uint8_t> decrypt(
        const uint8_t* ciphertext, size_t ciphertextLen,
        const uint8_t* key, size_t keyLen,
        const uint8_t* nonce, size_t nonceLen,
        const uint8_t* tag, size_t tagLen) override;

    void sha256(const uint8_t* data, size_t len, uint8_t* hash, size_t hashLen) override;

    bool sign(const uint8_t* data, size_t len,
              const uint8_t* privateKey, size_t privateKeyLen,
              uint8_t* signature, size_t signatureLen) override;

    bool verify(const uint8_t* data, size_t len,
                const uint8_t* publicKey, size_t publicKeyLen,
                const uint8_t* signature, size_t signatureLen) override;

    void generateKey(uint8_t* key, size_t keyLen) override;
    void generateNonce(uint8_t* nonce, size_t nonceLen) override;

    void generateKeyPair(uint8_t* publicKey, size_t publicKeyLen,
                         uint8_t* privateKey, size_t privateKeyLen) override;

    bool readPublicKey(const std::string& path, uint8_t* key, size_t keyLen) override;
    bool readPrivateKey(const std::string& path, uint8_t* key, size_t keyLen) override;
    bool writePublicKey(const std::string& path, const uint8_t* key, size_t keyLen) override;
    bool writePrivateKey(const std::string& path, const uint8_t* key, size_t keyLen) override;

    // --- Static wrappers (backward compat for internal archive code) ---
    static std::vector<uint8_t> encrypt(
        const uint8_t* plaintext, size_t plaintextLen,
        const uint8_t key[AES_KEY_SIZE],
        uint8_t nonce[AES_NONCE_SIZE],
        uint8_t tag[AES_TAG_SIZE]);

    static std::vector<uint8_t> decrypt(
        const uint8_t* ciphertext, size_t ciphertextLen,
        const uint8_t key[AES_KEY_SIZE],
        const uint8_t nonce[AES_NONCE_SIZE],
        const uint8_t tag[AES_TAG_SIZE]);

    static void sha256(const uint8_t* data, size_t len, uint8_t hash[PATH_HASH_SIZE]);

    static bool sign(const uint8_t* data, size_t len,
                     const uint8_t privateKey[64],
                     uint8_t signature[SIGNATURE_SIZE]);

    static bool verify(const uint8_t* data, size_t len,
                       const uint8_t publicKey[PUBLICKEY_SIZE],
                       const uint8_t signature[SIGNATURE_SIZE]);

    static void generateKey(uint8_t key[AES_KEY_SIZE]);
    static void generateNonce(uint8_t nonce[AES_NONCE_SIZE]);
    static void generateKeyPair(uint8_t publicKey[PUBLICKEY_SIZE],
                                uint8_t privateKey[64]);
    static bool readPublicKey(const std::string& path, uint8_t key[PUBLICKEY_SIZE]);
    static bool readPrivateKey(const std::string& path, uint8_t key[64]);
    static bool writePublicKey(const std::string& path, const uint8_t key[PUBLICKEY_SIZE]);
    static bool writePrivateKey(const std::string& path, const uint8_t key[64]);
};

} // namespace Caesura::carc
