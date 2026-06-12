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
// Singleton
// ==========================================================================

CryptoEngine& CryptoEngine::instance() {
    static CryptoEngine inst;
    return inst;
}

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
// Instance: AES-256-GCM encrypt
// ==========================================================================
std::vector<uint8_t> CryptoEngine::encrypt(
    const uint8_t* plaintext, size_t plaintextLen,
    const uint8_t* key, size_t keyLen,
    uint8_t* nonce, size_t nonceLen,
    uint8_t* tag, size_t tagLen)
{
    if (!plaintext || plaintextLen == 0 || !key || keyLen < 32) return {};

#ifdef _WIN32
    try {
        BcryptAlgHandle alg;
        checkBCrypt(BCryptOpenAlgorithmProvider(&alg.h, BCRYPT_AES_ALGORITHM, nullptr, 0), "OpenAlgorithmProvider(AES)");
        checkBCrypt(BCryptSetProperty(alg.h, BCRYPT_CHAINING_MODE,
                     (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0), "SetProperty(GCM)");

        struct { BCRYPT_KEY_DATA_BLOB_HEADER hdr; uint8_t data[32]; } blob;
        blob.hdr.dwMagic = BCRYPT_KEY_DATA_BLOB_MAGIC;
        blob.hdr.dwVersion = BCRYPT_KEY_DATA_BLOB_VERSION1;
        blob.hdr.cbKeyData = 32;
        memcpy(blob.data, key, 32);

        BcryptKeyHandle keyObj;
        checkBCrypt(BCryptImportKey(alg.h, nullptr, BCRYPT_KEY_DATA_BLOB, &keyObj.h, nullptr, 0,
                     (PUCHAR)&blob, sizeof(blob), 0), "ImportKey");

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth;
        BCRYPT_INIT_AUTH_MODE_INFO(auth);
        auth.pbNonce = nonce; auth.cbNonce = (ULONG)nonceLen;
        auth.pbTag   = tag;   auth.cbTag   = (ULONG)tagLen;

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
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)nonceLen, nullptr) != 1) break;
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, nonce) != 1) break;
        if (EVP_EncryptUpdate(ctx, out.data(), &len, plaintext, (int)plaintextLen) != 1) break;
        outLen = len;
        if (EVP_EncryptFinal_ex(ctx, out.data() + len, &len) != 1) break;
        outLen += len;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, (int)tagLen, tag) != 1) break;
        ok = true;
    } while (false);
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return {};
    out.resize(outLen);
    return out;
#endif
}

// ==========================================================================
// Instance: AES-256-GCM decrypt
// ==========================================================================
std::vector<uint8_t> CryptoEngine::decrypt(
    const uint8_t* ciphertext, size_t ciphertextLen,
    const uint8_t* key, size_t keyLen,
    const uint8_t* nonce, size_t nonceLen,
    const uint8_t* tag, size_t tagLen)
{
    if (!ciphertext || ciphertextLen == 0 || !key || keyLen < 32) return {};

#ifdef _WIN32
    try {
        BcryptAlgHandle alg;
        checkBCrypt(BCryptOpenAlgorithmProvider(&alg.h, BCRYPT_AES_ALGORITHM, nullptr, 0), "OpenAlgorithmProvider(AES)");
        checkBCrypt(BCryptSetProperty(alg.h, BCRYPT_CHAINING_MODE,
                     (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0), "SetProperty(GCM)");

        struct { BCRYPT_KEY_DATA_BLOB_HEADER hdr; uint8_t data[32]; } blob;
        blob.hdr.dwMagic = BCRYPT_KEY_DATA_BLOB_MAGIC;
        blob.hdr.dwVersion = BCRYPT_KEY_DATA_BLOB_VERSION1;
        blob.hdr.cbKeyData = 32;
        memcpy(blob.data, key, 32);

        BcryptKeyHandle keyObj;
        checkBCrypt(BCryptImportKey(alg.h, nullptr, BCRYPT_KEY_DATA_BLOB, &keyObj.h, nullptr, 0,
                     (PUCHAR)&blob, sizeof(blob), 0), "ImportKey");

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth;
        BCRYPT_INIT_AUTH_MODE_INFO(auth);
        auth.pbNonce = const_cast<uint8_t*>(nonce); auth.cbNonce = (ULONG)nonceLen;
        auth.pbTag   = const_cast<uint8_t*>(tag);   auth.cbTag   = (ULONG)tagLen;

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
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)nonceLen, nullptr) != 1) break;
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key, nonce) != 1) break;
        if (EVP_DecryptUpdate(ctx, out.data(), &len, ciphertext, (int)ciphertextLen) != 1) break;
        outLen = len;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, (int)tagLen, const_cast<uint8_t*>(tag)) != 1) break;
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
// Instance: SHA-256
// ==========================================================================
void CryptoEngine::sha256(const uint8_t* data, size_t len, uint8_t* hash, size_t hashLen)
{
    (void)hashLen;
#ifdef _WIN32
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE h  = nullptr;
    try {
        checkBCrypt(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0), "OpenAlgorithmProvider(SHA256)");
        checkBCrypt(BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0), "CreateHash");
        checkBCrypt(BCryptHashData(h, (PUCHAR)data, (ULONG)len, 0), "HashData");
        checkBCrypt(BCryptFinishHash(h, hash, (ULONG)hashLen, 0), "FinishHash");
    } catch (const std::exception&) { memset(hash, 0, hashLen); }
    if (h)   BCryptDestroyHash(h);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
#else
    unsigned int outLen = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx) {
        EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
        EVP_DigestUpdate(ctx, data, len);
        EVP_DigestFinal_ex(ctx, hash, &outLen);
        EVP_MD_CTX_free(ctx);
    }
    if (outLen != hashLen) memset(hash, 0, hashLen);
#endif
}

// ==========================================================================
// Instance: Ed25519 signing / verification (orlp -- cross-platform)
// ==========================================================================
bool CryptoEngine::sign(const uint8_t* data, size_t len,
                        const uint8_t* privateKey, size_t privateKeyLen,
                        uint8_t* signature, size_t signatureLen)
{
    (void)privateKeyLen; (void)signatureLen;
    ed25519_sign(signature, data, len, privateKey + 32, privateKey);
    return true;
}

bool CryptoEngine::verify(const uint8_t* data, size_t len,
                          const uint8_t* publicKey, size_t publicKeyLen,
                          const uint8_t* signature, size_t signatureLen)
{
    (void)publicKeyLen; (void)signatureLen;
    return ed25519_verify(signature, data, len, publicKey) != 0;
}

// ==========================================================================
// Instance: Random generation
// ==========================================================================
void CryptoEngine::generateKey(uint8_t* key, size_t keyLen)
{
#ifdef _WIN32
    if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, key, (ULONG)keyLen, BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
        throw std::runtime_error("BCryptGenRandom failed for key");
#else
    if (RAND_bytes(key, (int)keyLen) != 1)
        throw std::runtime_error("RAND_bytes failed for key");
#endif
}

void CryptoEngine::generateNonce(uint8_t* nonce, size_t nonceLen)
{
#ifdef _WIN32
    if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, nonce, (ULONG)nonceLen, BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
        throw std::runtime_error("BCryptGenRandom failed for nonce");
#else
    if (RAND_bytes(nonce, (int)nonceLen) != 1)
        throw std::runtime_error("RAND_bytes failed for nonce");
#endif
}

// ==========================================================================
// Instance: Key pair generation
// ==========================================================================
void CryptoEngine::generateKeyPair(uint8_t* publicKey, size_t publicKeyLen,
                                   uint8_t* privateKey, size_t privateKeyLen)
{
    (void)publicKeyLen; (void)privateKeyLen;
    uint8_t seed[32];
    generateKey(seed, sizeof(seed));
    ed25519_create_keypair(publicKey, privateKey, seed);
}

// ==========================================================================
// Instance: Key file I/O
// ==========================================================================
bool CryptoEngine::readPublicKey(const std::string& path, uint8_t* key, size_t keyLen)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.read(reinterpret_cast<char*>(key), (std::streamsize)keyLen);
    return (size_t)f.gcount() == keyLen;
}

bool CryptoEngine::readPrivateKey(const std::string& path, uint8_t* key, size_t keyLen)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.read(reinterpret_cast<char*>(key), (std::streamsize)keyLen);
    return (size_t)f.gcount() == keyLen;
}

bool CryptoEngine::writePublicKey(const std::string& path, const uint8_t* key, size_t keyLen)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(key), (std::streamsize)keyLen);
    return f.good();
}

bool CryptoEngine::writePrivateKey(const std::string& path, const uint8_t* key, size_t keyLen)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(key), (std::streamsize)keyLen);
    return f.good();
}

// ==========================================================================
// Static wrappers (delegate to instance)
// ==========================================================================

std::vector<uint8_t> CryptoEngine::encrypt(
    const uint8_t* plaintext, size_t plaintextLen,
    const uint8_t key[AES_KEY_SIZE],
    uint8_t nonce[AES_NONCE_SIZE],
    uint8_t tag[AES_TAG_SIZE])
{
    return instance().encrypt(plaintext, plaintextLen, key, AES_KEY_SIZE, nonce, AES_NONCE_SIZE, tag, AES_TAG_SIZE);
}

std::vector<uint8_t> CryptoEngine::decrypt(
    const uint8_t* ciphertext, size_t ciphertextLen,
    const uint8_t key[AES_KEY_SIZE],
    const uint8_t nonce[AES_NONCE_SIZE],
    const uint8_t tag[AES_TAG_SIZE])
{
    return instance().decrypt(ciphertext, ciphertextLen, key, AES_KEY_SIZE, nonce, AES_NONCE_SIZE, tag, AES_TAG_SIZE);
}

void CryptoEngine::sha256(const uint8_t* data, size_t len, uint8_t hash[PATH_HASH_SIZE])
{
    instance().sha256(data, len, hash, PATH_HASH_SIZE);
}

bool CryptoEngine::sign(const uint8_t* data, size_t len,
                        const uint8_t privateKey[64],
                        uint8_t signature[SIGNATURE_SIZE])
{
    return instance().sign(data, len, privateKey, 64, signature, SIGNATURE_SIZE);
}

bool CryptoEngine::verify(const uint8_t* data, size_t len,
                          const uint8_t publicKey[PUBLICKEY_SIZE],
                          const uint8_t signature[SIGNATURE_SIZE])
{
    return instance().verify(data, len, publicKey, PUBLICKEY_SIZE, signature, SIGNATURE_SIZE);
}

void CryptoEngine::generateKey(uint8_t key[AES_KEY_SIZE])
{
    instance().generateKey(key, AES_KEY_SIZE);
}

void CryptoEngine::generateNonce(uint8_t nonce[AES_NONCE_SIZE])
{
    instance().generateNonce(nonce, AES_NONCE_SIZE);
}

void CryptoEngine::generateKeyPair(uint8_t publicKey[PUBLICKEY_SIZE],
                                   uint8_t privateKey[64])
{
    instance().generateKeyPair(publicKey, PUBLICKEY_SIZE, privateKey, 64);
}

bool CryptoEngine::readPublicKey(const std::string& path, uint8_t key[PUBLICKEY_SIZE])
{
    return instance().readPublicKey(path, key, PUBLICKEY_SIZE);
}

bool CryptoEngine::readPrivateKey(const std::string& path, uint8_t key[64])
{
    return instance().readPrivateKey(path, key, 64);
}

bool CryptoEngine::writePublicKey(const std::string& path, const uint8_t key[PUBLICKEY_SIZE])
{
    return instance().writePublicKey(path, key, PUBLICKEY_SIZE);
}

bool CryptoEngine::writePrivateKey(const std::string& path, const uint8_t key[64])
{
    return instance().writePrivateKey(path, key, 64);
}

} // namespace Caesura::carc
