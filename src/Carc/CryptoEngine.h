// CryptoEngine -- Windows: BCrypt, macOS/Linux: OpenSSL (optional via CAESURA_CRYPTO_OPENSSL).
// Ed25519 via orlp library (portable).
#pragma once
#include "CARCFormat.h"
#include <vector>
#include <string>
#include <cstdint>

namespace caesura::carc {

class CryptoEngine {
public:
    // --- AES-256-GCM ---
    // On success, returns encrypted/decrypted data.
    // On failure, returns empty vector.
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

    // --- SHA-256 ---
    static void sha256(const uint8_t* data, size_t len, uint8_t hash[PATH_HASH_SIZE]);

    // --- Ed25519 signing / verification ---
    // Keys are raw binary files (public: 32 bytes, private: 64 bytes expanded).
    static bool sign(const uint8_t* data, size_t len,
                     const uint8_t privateKey[64],
                     uint8_t signature[SIGNATURE_SIZE]);

    static bool verify(const uint8_t* data, size_t len,
                       const uint8_t publicKey[PUBLICKEY_SIZE],
                       const uint8_t signature[SIGNATURE_SIZE]);

    // --- Random key generation ---
    static void generateKey(uint8_t key[AES_KEY_SIZE]);
    static void generateNonce(uint8_t nonce[AES_NONCE_SIZE]);

    // --- Key pair generation (returns seed + public key) ---
    // seed: 32 bytes, publicKey: 32 bytes, privateKey (expanded): 64 bytes
    static void generateKeyPair(uint8_t publicKey[PUBLICKEY_SIZE],
                                uint8_t privateKey[64]);

    // --- Key file I/O helpers ---
    static bool readPublicKey(const std::string& path, uint8_t key[PUBLICKEY_SIZE]);
    static bool readPrivateKey(const std::string& path, uint8_t key[64]);
    static bool writePublicKey(const std::string& path, const uint8_t key[PUBLICKEY_SIZE]);
    static bool writePrivateKey(const std::string& path, const uint8_t key[64]);
};

} // namespace caesura::carc