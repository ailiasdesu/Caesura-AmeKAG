// CryptoEngine -- AES-256-GCM + SHA-256.
//   Windows:  BCrypt (zero system deps, thread-safe)
//   macOS/Linux: OpenSSL EVP
// Ed25519 via orlp library (cross-platform).
#include "CryptoEngine.h"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

#include <cstring>
#include <fstream>
#include <stdexcept>

extern "C" {
#include "../../external/ed25519/ed25519.h"
}

namespace Caesura::carc {

// ==========================================================================
// Helpers
// ==========================================================================

#ifdef _WIN32
namespace {

void checkBCrypt(NTSTATUS status, const char* op) {
    if (!BCRYPT_SUCCESS(status)) {
        throw std::runtime_error(std::string("BCrypt ") + op + " failed: 0x"
                                 + std::to_string(static_cast<uint32_t>(status)));
    }
}

struct BcryptAlgHandle { BCRYPT_ALG_HANDLE h = nullptr; ~BcryptAlgHandle() { if (h) BCryptCloseAlgorithmProvider(h, 0); } };
struct BcryptKeyHandle  { BCRYPT_KEY_HANDLE h = nullptr;  ~BcryptKeyHandle()  { if (h) BCryptDestroyKey(h); } };

} // anon
#endif

// ==========================================================================
// AES-256-GCM encrypt
// ==========================================================================
std::vector<uint8_t> CryptoEngine::encrypt(
    const uint8_t* plaintext, size_t plaintextLen,
    const uint8_t key[AES_KEY_SIZE],
    uint8_t nonce[AES_NONCE_SIZE],
    uint8_t tag[AES_TAG_SIZE])
{
    if (!plaintext || plaintextLen == 0) return {};

#ifdef _WIN32
    try {
        BcryptAlgHandle alg;
        checkBCrypt(BCryptOpenAlgorithmProvider(&alg.h, BCRYPT_AES_ALGORITHM, nullptr, 0), "OpenAlgorithmProvider(AES)");
        checkBCrypt(BCryptSetProperty(alg.h, BCRYPT_CHAINING_MODE,
                     (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0), "SetProperty(GCM)");

        struct { BCRYPT_KEY_DATA_BLOB_HEADER hdr; uint8_t data[AES_KEY_SIZE]; } blob;
        blob.hdr.dwMagic = BCRYPT_KEY_DATA_BLOB_MAGIC;
        blob.hdr.dwVersion = BCRYPT_KEY_DATA_BLOB_VERSION1;
        blob.hdr.cbKeyData = AES_KEY_SIZE;
        memcpy(blob.data, key, AES_KEY_SIZE);

        BcryptKeyHandle keyObj;
        checkBCrypt(BCryptImportKey(alg.h, nullptr, BCRYPT_KEY_DATA_BLOB, &keyObj.h, nullptr, 0,
                     (PUCHAR)&blob, sizeof(blob), 0), "ImportKey");

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth;
        BCRYPT_INIT_AUTH_MODE_INFO(auth);
        auth.pbNonce = nonce; auth.cbNonce = AES_NONCE_SIZE;
        auth.pbTag   = tag;   auth.cbTag   = AES_TAG_SIZE;

        std::vector<uint8_t> out(plaintextLen);
        ULONG done = 0;
        checkBCrypt(BCryptEncrypt(keyObj.h, (PUCHAR)plaintext, (ULONG)plaintextLen,
                     &auth, nullptr, 0, out.data(), (ULONG)plaintextLen, &done, 0), "Encrypt");
        out.resize(done);
        return out;
    } catch (const std::exception&) { return {}; }
#else
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    int len = 0;
    std::vector<uint8_t> out(plaintextLen + 16);
    int outLen = 0;
    bool ok = false;
    do {
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, AES_NONCE_SIZE, nullptr) != 1) break;
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, nonce) != 1) break;
        if (EVP_EncryptUpdate(ctx, out.data(), &len, plaintext, (int)plaintextLen) != 1) break;
        outLen = len;
        if (EVP_EncryptFinal_ex(ctx, out.data() + len, &len) != 1) break;
        outLen += len;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, AES_TAG_SIZE, tag) != 1) break;
        ok = true;
    } while (false);
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return {};
    out.resize(outLen);
    return out;
#endif
}

// ==========================================================================
// AES-256-GCM decrypt
// ==========================================================================
std::vector<uint8_t> CryptoEngine::decrypt(
    const uint8_t* ciphertext, size_t ciphertextLen,
    const uint8_t key[AES_KEY_SIZE],
    const uint8_t nonce[AES_NONCE_SIZE],
    const uint8_t tag[AES_TAG_SIZE])
{
    if (!ciphertext || ciphertextLen == 0) return {};

#ifdef _WIN32
    try {
        BcryptAlgHandle alg;
        checkBCrypt(BCryptOpenAlgorithmProvider(&alg.h, BCRYPT_AES_ALGORITHM, nullptr, 0), "OpenAlgorithmProvider(AES)");
        checkBCrypt(BCryptSetProperty(alg.h, BCRYPT_CHAINING_MODE,
                     (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0), "SetProperty(GCM)");

        struct { BCRYPT_KEY_DATA_BLOB_HEADER hdr; uint8_t data[AES_KEY_SIZE]; } blob;
        blob.hdr.dwMagic = BCRYPT_KEY_DATA_BLOB_MAGIC;
        blob.hdr.dwVersion = BCRYPT_KEY_DATA_BLOB_VERSION1;
        blob.hdr.cbKeyData = AES_KEY_SIZE;
        memcpy(blob.data, key, AES_KEY_SIZE);

        BcryptKeyHandle keyObj;
        checkBCrypt(BCryptImportKey(alg.h, nullptr, BCRYPT_KEY_DATA_BLOB, &keyObj.h, nullptr, 0,
                     (PUCHAR)&blob, sizeof(blob), 0), "ImportKey");

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth;
        BCRYPT_INIT_AUTH_MODE_INFO(auth);
        auth.pbNonce = const_cast<uint8_t*>(nonce); auth.cbNonce = AES_NONCE_SIZE;
        auth.pbTag   = const_cast<uint8_t*>(tag);   auth.cbTag   = AES_TAG_SIZE;

        std::vector<uint8_t> out(ciphertextLen);
        ULONG done = 0;
        checkBCrypt(BCryptDecrypt(keyObj.h, (PUCHAR)ciphertext, (ULONG)ciphertextLen,
                     &auth, nullptr, 0, out.data(), (ULONG)ciphertextLen, &done, 0), "Decrypt");
        out.resize(done);
        return out;
    } catch (const std::exception&) { return {}; }
#else
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    int len = 0;
    std::vector<uint8_t> out(ciphertextLen);
    int outLen = 0;
    bool ok = false;
    do {
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, AES_NONCE_SIZE, nullptr) != 1) break;
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key, nonce) != 1) break;
        if (EVP_DecryptUpdate(ctx, out.data(), &len, ciphertext, (int)ciphertextLen) != 1) break;
        outLen = len;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, AES_TAG_SIZE, const_cast<uint8_t*>(tag)) != 1) break;
        if (EVP_DecryptFinal_ex(ctx, out.data() + len, &len) != 1) break;
        outLen += len;
        ok = true;
    } while (false);
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return {};
    out.resize(outLen);
    return out;
#endif
}

// ==========================================================================
// SHA-256
// ==========================================================================
void CryptoEngine::sha256(const uint8_t* data, size_t len, uint8_t hash[PATH_HASH_SIZE])
{
#ifdef _WIN32
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE h  = nullptr;
    try {
        checkBCrypt(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0), "OpenAlgorithmProvider(SHA256)");
        checkBCrypt(BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0), "CreateHash");
        checkBCrypt(BCryptHashData(h, (PUCHAR)data, (ULONG)len, 0), "HashData");
        checkBCrypt(BCryptFinishHash(h, hash, PATH_HASH_SIZE, 0), "FinishHash");
    } catch (const std::exception&) { memset(hash, 0, PATH_HASH_SIZE); }
    if (h)   BCryptDestroyHash(h);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
#else
    unsigned int hashLen = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx) {
        EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
        EVP_DigestUpdate(ctx, data, len);
        EVP_DigestFinal_ex(ctx, hash, &hashLen);
        EVP_MD_CTX_free(ctx);
    }
    if (hashLen != PATH_HASH_SIZE) memset(hash, 0, PATH_HASH_SIZE);
#endif
}

// ==========================================================================
// Ed25519 signing / verification (orlp — cross-platform)
// ==========================================================================
bool CryptoEngine::sign(const uint8_t* data, size_t len,
                        const uint8_t privateKey[64],
                        uint8_t signature[SIGNATURE_SIZE])
{
    ed25519_sign(signature, data, len, privateKey + 32, privateKey);
    return true;
}

bool CryptoEngine::verify(const uint8_t* data, size_t len,
                          const uint8_t publicKey[PUBLICKEY_SIZE],
                          const uint8_t signature[SIGNATURE_SIZE])
{
    return ed25519_verify(signature, data, len, publicKey) != 0;
}

// ==========================================================================
// Random generation
// ==========================================================================
void CryptoEngine::generateKey(uint8_t key[AES_KEY_SIZE])
{
#ifdef _WIN32
    if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, key, AES_KEY_SIZE, BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
        throw std::runtime_error("BCryptGenRandom failed for AES key");
#else
    if (RAND_bytes(key, AES_KEY_SIZE) != 1)
        throw std::runtime_error("RAND_bytes failed for AES key");
#endif
}

void CryptoEngine::generateNonce(uint8_t nonce[AES_NONCE_SIZE])
{
#ifdef _WIN32
    if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, nonce, AES_NONCE_SIZE, BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
        throw std::runtime_error("BCryptGenRandom failed for nonce");
#else
    if (RAND_bytes(nonce, AES_NONCE_SIZE) != 1)
        throw std::runtime_error("RAND_bytes failed for nonce");
#endif
}

// ==========================================================================
// Key pair generation
// ==========================================================================
void CryptoEngine::generateKeyPair(uint8_t publicKey[PUBLICKEY_SIZE],
                                   uint8_t privateKey[64])
{
    uint8_t seed[32];
    generateKey(seed);
    ed25519_create_keypair(publicKey, privateKey, seed);
}

// ==========================================================================
// Key file I/O (std::ifstream — cross-platform, unified)
// ==========================================================================
bool CryptoEngine::readPublicKey(const std::string& path, uint8_t key[PUBLICKEY_SIZE])
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.read(reinterpret_cast<char*>(key), PUBLICKEY_SIZE);
    return f.gcount() == PUBLICKEY_SIZE;
}

bool CryptoEngine::readPrivateKey(const std::string& path, uint8_t key[64])
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.read(reinterpret_cast<char*>(key), 64);
    return f.gcount() == 64;
}

bool CryptoEngine::writePublicKey(const std::string& path, const uint8_t key[PUBLICKEY_SIZE])
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(key), PUBLICKEY_SIZE);
    return f.good();
}

bool CryptoEngine::writePrivateKey(const std::string& path, const uint8_t key[64])
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(key), 64);
    return f.good();
}

} // namespace Caesura::carc