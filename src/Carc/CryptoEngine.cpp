// CryptoEngine  Windows BCrypt implementation
#include "CryptoEngine.h"

#include <windows.h>
#include <bcrypt.h>
#include <cstring>
#include <random>
#include <stdexcept>

extern "C" {
#include "../../external/ed25519/ed25519.h"
}

#pragma comment(lib, "bcrypt.lib")

namespace caesura::carc {

namespace {

void checkBCrypt(NTSTATUS status, const char* op) {
    if (!BCRYPT_SUCCESS(status)) {
        throw std::runtime_error(std::string("BCrypt ") + op + " failed: 0x"
                                 + std::to_string(static_cast<uint32_t>(status)));
    }
}

struct BcryptHandle {
    BCRYPT_ALG_HANDLE h = nullptr;
    ~BcryptHandle() { if (h) BCryptCloseAlgorithmProvider(h, 0); }
};

struct BcryptKeyHandle {
    BCRYPT_KEY_HANDLE h = nullptr;
    ~BcryptKeyHandle() { if (h) BCryptDestroyKey(h); }
};

} // anon

// ==========================================================================
// AES-256-GCM encrypt
// ==========================================================================
std::vector<uint8_t> CryptoEngine::encrypt(
    const uint8_t* plaintext, size_t plaintextLen,
    const uint8_t key[AES_KEY_SIZE],
    uint8_t nonce[AES_NONCE_SIZE],
    uint8_t tag[AES_TAG_SIZE])
{
    try {
        BcryptHandle alg;
        checkBCrypt(BCryptOpenAlgorithmProvider(&alg.h, BCRYPT_AES_ALGORITHM, nullptr, 0),
                    "BCryptOpenAlgorithmProvider(AES)");
        checkBCrypt(BCryptSetProperty(alg.h, BCRYPT_CHAINING_MODE,
                     (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0),
                    "BCryptSetProperty(GCM)");

        BcryptKeyHandle keyObj;
        struct KeyBlob {
            BCRYPT_KEY_DATA_BLOB_HEADER hdr;
            uint8_t keyData[AES_KEY_SIZE];
        } keyBlob;
        keyBlob.hdr.dwMagic = BCRYPT_KEY_DATA_BLOB_MAGIC;
        keyBlob.hdr.dwVersion = BCRYPT_KEY_DATA_BLOB_VERSION1;
        keyBlob.hdr.cbKeyData = AES_KEY_SIZE;
        memcpy(keyBlob.keyData, key, AES_KEY_SIZE);

        checkBCrypt(BCryptImportKey(alg.h, nullptr, BCRYPT_KEY_DATA_BLOB,
                     &keyObj.h, nullptr, 0, (PUCHAR)&keyBlob, sizeof(keyBlob), 0),
                    "BCryptImportKey");

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        authInfo.pbNonce = nonce;
        authInfo.cbNonce = AES_NONCE_SIZE;
        authInfo.pbTag = tag;
        authInfo.cbTag = AES_TAG_SIZE;

        std::vector<uint8_t> ciphertext(plaintextLen);
        ULONG cbResult = 0;
        checkBCrypt(BCryptEncrypt(keyObj.h, (PUCHAR)plaintext, (ULONG)plaintextLen,
                     &authInfo, nullptr, 0, ciphertext.data(), (ULONG)plaintextLen,
                     &cbResult, 0),
                    "BCryptEncrypt");
        ciphertext.resize(cbResult);
        return ciphertext;
    } catch (const std::exception&) {
        return {};
    }
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
    try {
        BcryptHandle alg;
        checkBCrypt(BCryptOpenAlgorithmProvider(&alg.h, BCRYPT_AES_ALGORITHM, nullptr, 0),
                    "BCryptOpenAlgorithmProvider(AES)");
        checkBCrypt(BCryptSetProperty(alg.h, BCRYPT_CHAINING_MODE,
                     (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0),
                    "BCryptSetProperty(GCM)");

        BcryptKeyHandle keyObj;
        struct KeyBlob {
            BCRYPT_KEY_DATA_BLOB_HEADER hdr;
            uint8_t keyData[AES_KEY_SIZE];
        } keyBlob;
        keyBlob.hdr.dwMagic = BCRYPT_KEY_DATA_BLOB_MAGIC;
        keyBlob.hdr.dwVersion = BCRYPT_KEY_DATA_BLOB_VERSION1;
        keyBlob.hdr.cbKeyData = AES_KEY_SIZE;
        memcpy(keyBlob.keyData, key, AES_KEY_SIZE);

        checkBCrypt(BCryptImportKey(alg.h, nullptr, BCRYPT_KEY_DATA_BLOB,
                     &keyObj.h, nullptr, 0, (PUCHAR)&keyBlob, sizeof(keyBlob), 0),
                    "BCryptImportKey");

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        authInfo.pbNonce = const_cast<uint8_t*>(nonce);
        authInfo.cbNonce = AES_NONCE_SIZE;
        authInfo.pbTag = const_cast<uint8_t*>(tag);
        authInfo.cbTag = AES_TAG_SIZE;

        std::vector<uint8_t> plaintext(ciphertextLen);
        ULONG cbResult = 0;
        checkBCrypt(BCryptDecrypt(keyObj.h, (PUCHAR)ciphertext, (ULONG)ciphertextLen,
                     &authInfo, nullptr, 0, plaintext.data(), (ULONG)ciphertextLen,
                     &cbResult, 0),
                    "BCryptDecrypt");
        plaintext.resize(cbResult);
        return plaintext;
    } catch (const std::exception&) {
        return {};
    }
}

// ==========================================================================
// SHA-256 via BCrypt
// ==========================================================================
void CryptoEngine::sha256(const uint8_t* data, size_t len, uint8_t hash[PATH_HASH_SIZE])
{
    BcryptHandle alg;
    checkBCrypt(BCryptOpenAlgorithmProvider(&alg.h, BCRYPT_SHA256_ALGORITHM, nullptr, 0),
                "BCryptOpenAlgorithmProvider(SHA256)");

    DWORD cbHashObject = 0, cbData = 0;
    checkBCrypt(BCryptGetProperty(alg.h, BCRYPT_OBJECT_LENGTH,
                 (PUCHAR)&cbHashObject, sizeof(DWORD), &cbData, 0),
                "BCryptGetProperty(OBJECT_LENGTH)");

    std::vector<uint8_t> hashObject(cbHashObject);
    BcryptKeyHandle hashHandle;
    checkBCrypt(BCryptCreateHash(alg.h, &hashHandle.h, hashObject.data(),
                 cbHashObject, nullptr, 0, 0),
                "BCryptCreateHash");
    checkBCrypt(BCryptHashData(hashHandle.h, (PUCHAR)data, (ULONG)len, 0),
                "BCryptHashData");
    checkBCrypt(BCryptFinishHash(hashHandle.h, hash, PATH_HASH_SIZE, 0),
                "BCryptFinishHash");
}

// ==========================================================================
// Ed25519 sign / verify
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
// Random key generation
// ==========================================================================
void CryptoEngine::generateKey(uint8_t key[AES_KEY_SIZE])
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned int> dist(0, 255);
    for (size_t i = 0; i < AES_KEY_SIZE; ++i)
        key[i] = static_cast<uint8_t>(dist(gen));
}

void CryptoEngine::generateNonce(uint8_t nonce[AES_NONCE_SIZE])
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned int> dist(0, 255);
    for (size_t i = 0; i < AES_NONCE_SIZE; ++i)
        nonce[i] = static_cast<uint8_t>(dist(gen));
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
// Key file I/O
// ==========================================================================
static bool readFileBytes(HANDLE hFile, void* buf, DWORD size) {
    DWORD read = 0;
    return ReadFile(hFile, buf, size, &read, nullptr) && read == size;
}

bool CryptoEngine::readPublicKey(const std::string& path, uint8_t key[PUBLICKEY_SIZE])
{
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    bool ok = readFileBytes(hFile, key, PUBLICKEY_SIZE);
    CloseHandle(hFile);
    return ok;
}

bool CryptoEngine::readPrivateKey(const std::string& path, uint8_t key[64])
{
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    bool ok = readFileBytes(hFile, key, 64);
    CloseHandle(hFile);
    return ok;
}

bool CryptoEngine::writePublicKey(const std::string& path, const uint8_t key[PUBLICKEY_SIZE])
{
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0,
                               nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(hFile, key, PUBLICKEY_SIZE, &written, nullptr) && written == PUBLICKEY_SIZE;
    CloseHandle(hFile);
    return ok;
}

bool CryptoEngine::writePrivateKey(const std::string& path, const uint8_t key[64])
{
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0,
                               nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(hFile, key, 64, &written, nullptr) && written == 64;
    CloseHandle(hFile);
    return ok;
}

} // namespace caesura::carc