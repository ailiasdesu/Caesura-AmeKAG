// CryptoEngine — Windows: BCrypt, macOS/Linux: OpenSSL
// Ed25519 via orlp library (portable).
#include "CryptoEngine.h"
#include <cstring>
#include <stdexcept>
#include <cstdio>

#if defined(_WIN32)
    #include <windows.h>
    #include <bcrypt.h>
    #pragma comment(lib, "bcrypt.lib")
#elif defined(CAESURA_CRYPTO_OPENSSL)
    #include <openssl/evp.h>
    #include <openssl/rand.h>
#endif

extern "C" {
#include "../../external/ed25519/src/ed25519.h"
}

namespace caesura::carc {

#if defined(_WIN32)
namespace {
void checkBCrypt(NTSTATUS s, const char* op) {
    if (!BCRYPT_SUCCESS(s)) throw std::runtime_error(std::string("BCrypt ")+op+" failed");
}
struct BcryptHandle  { BCRYPT_ALG_HANDLE h = nullptr; ~BcryptHandle()  { if(h) BCryptCloseAlgorithmProvider(h,0); } };
struct BcryptKeyHandle { BCRYPT_KEY_HANDLE h = nullptr; ~BcryptKeyHandle() { if(h) BCryptDestroyKey(h); } };
}
#endif

// AES-256-GCM encrypt
std::vector<uint8_t> CryptoEngine::encrypt(
    const uint8_t* pt, size_t ptLen, const uint8_t key[AES_KEY_SIZE],
    uint8_t nonce[AES_NONCE_SIZE], uint8_t tag[AES_TAG_SIZE])
{
#if defined(_WIN32)
    try {
        BcryptHandle alg; checkBCrypt(BCryptOpenAlgorithmProvider(&alg.h,BCRYPT_AES_ALGORITHM,nullptr,0),"open");
        checkBCrypt(BCryptSetProperty(alg.h,BCRYPT_CHAINING_MODE,(PUCHAR)BCRYPT_CHAIN_MODE_GCM,sizeof(BCRYPT_CHAIN_MODE_GCM),0),"gcm");
        BcryptKeyHandle k; struct{BCRYPT_KEY_DATA_BLOB_HEADER h;uint8_t d[32];}kb={{BCRYPT_KEY_DATA_BLOB_MAGIC,BCRYPT_KEY_DATA_BLOB_VERSION1,32}};
        memcpy(kb.d,key,32); checkBCrypt(BCryptImportKey(alg.h,nullptr,BCRYPT_KEY_DATA_BLOB,&k.h,nullptr,0,(PUCHAR)&kb,sizeof(kb),0),"import");
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO a; BCRYPT_INIT_AUTH_MODE_INFO(a);
        a.pbNonce=nonce; a.cbNonce=12; a.pbTag=tag; a.cbTag=16;
        std::vector<uint8_t> ct(ptLen); ULONG r=0;
        checkBCrypt(BCryptEncrypt(k.h,(PUCHAR)pt,(ULONG)ptLen,&a,nullptr,0,ct.data(),(ULONG)ptLen,&r,0),"enc");
        ct.resize(r); return ct;
    } catch(...) { return {}; }
#elif defined(CAESURA_CRYPTO_OPENSSL)
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new(); if(!ctx) return {};
    int len=0, clen=0; std::vector<uint8_t> ct(ptLen+16); bool ok=false;
    do {
        if(EVP_EncryptInit_ex(ctx,EVP_aes_256_gcm(),nullptr,nullptr,nullptr)!=1) break;
        if(EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_GCM_SET_IVLEN,12,nullptr)!=1) break;
        if(EVP_EncryptInit_ex(ctx,nullptr,nullptr,key,nonce)!=1) break;
        if(EVP_EncryptUpdate(ctx,ct.data(),&len,pt,(int)ptLen)!=1) break;
        clen=len;
        if(EVP_EncryptFinal_ex(ctx,ct.data()+len,&len)!=1) break;
        clen+=len;
        if(EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_GCM_GET_TAG,16,tag)!=1) break;
        ok=true;
    } while(false);
    EVP_CIPHER_CTX_free(ctx);
    if(!ok) { fprintf(stderr,"[CryptoEngine] OpenSSL encrypt failed\n"); return {}; }
    ct.resize(clen); return ct;
#else
    fprintf(stderr,"[CryptoEngine] encrypt: no backend\n"); return {};
#endif
}

// AES-256-GCM decrypt
std::vector<uint8_t> CryptoEngine::decrypt(
    const uint8_t* ct, size_t ctLen, const uint8_t key[AES_KEY_SIZE],
    const uint8_t nonce[AES_NONCE_SIZE], const uint8_t tag[AES_TAG_SIZE])
{
#if defined(_WIN32)
    try {
        BcryptHandle alg; checkBCrypt(BCryptOpenAlgorithmProvider(&alg.h,BCRYPT_AES_ALGORITHM,nullptr,0),"open");
        checkBCrypt(BCryptSetProperty(alg.h,BCRYPT_CHAINING_MODE,(PUCHAR)BCRYPT_CHAIN_MODE_GCM,sizeof(BCRYPT_CHAIN_MODE_GCM),0),"gcm");
        BcryptKeyHandle k; struct{BCRYPT_KEY_DATA_BLOB_HEADER h;uint8_t d[32];}kb={{BCRYPT_KEY_DATA_BLOB_MAGIC,BCRYPT_KEY_DATA_BLOB_VERSION1,32}};
        memcpy(kb.d,key,32); checkBCrypt(BCryptImportKey(alg.h,nullptr,BCRYPT_KEY_DATA_BLOB,&k.h,nullptr,0,(PUCHAR)&kb,sizeof(kb),0),"import");
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO a; BCRYPT_INIT_AUTH_MODE_INFO(a);
        a.pbNonce=const_cast<uint8_t*>(nonce); a.cbNonce=12; a.pbTag=const_cast<uint8_t*>(tag); a.cbTag=16;
        std::vector<uint8_t> pt(ctLen); ULONG r=0;
        checkBCrypt(BCryptDecrypt(k.h,(PUCHAR)ct,(ULONG)ctLen,&a,nullptr,0,pt.data(),(ULONG)ctLen,&r,0),"dec");
        pt.resize(r); return pt;
    } catch(...) { return {}; }
#elif defined(CAESURA_CRYPTO_OPENSSL)
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new(); if(!ctx) return {};
    int len=0, plen=0; std::vector<uint8_t> pt(ctLen+16); bool ok=false;
    do {
        if(EVP_DecryptInit_ex(ctx,EVP_aes_256_gcm(),nullptr,nullptr,nullptr)!=1) break;
        if(EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_GCM_SET_IVLEN,12,nullptr)!=1) break;
        if(EVP_DecryptInit_ex(ctx,nullptr,nullptr,key,nonce)!=1) break;
        if(EVP_DecryptUpdate(ctx,pt.data(),&len,ct,(int)ctLen)!=1) break;
        plen=len;
        if(EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_GCM_SET_TAG,16,const_cast<uint8_t*>(tag))!=1) break;
        if(EVP_DecryptFinal_ex(ctx,pt.data()+len,&len)<=0) { EVP_CIPHER_CTX_free(ctx); fprintf(stderr,"[CryptoEngine] decrypt: auth failed\n"); return {}; }
        plen+=len; ok=true;
    } while(false);
    EVP_CIPHER_CTX_free(ctx);
    if(!ok) { fprintf(stderr,"[CryptoEngine] OpenSSL decrypt failed\n"); return {}; }
    pt.resize(plen); return pt;
#else
    fprintf(stderr,"[CryptoEngine] decrypt: no backend\n"); return {};
#endif
}

// SHA-256
void CryptoEngine::sha256(const uint8_t* data, size_t len, uint8_t hash[PATH_HASH_SIZE])
{
#if defined(_WIN32)
    BcryptHandle alg; checkBCrypt(BCryptOpenAlgorithmProvider(&alg.h,BCRYPT_SHA256_ALGORITHM,nullptr,0),"sha256");
    DWORD objLen=0, cb=0; checkBCrypt(BCryptGetProperty(alg.h,BCRYPT_OBJECT_LENGTH,(PUCHAR)&objLen,sizeof(DWORD),&cb,0),"objlen");
    std::vector<uint8_t> obj(objLen); BcryptKeyHandle hh;
    checkBCrypt(BCryptCreateHash(alg.h,&hh.h,obj.data(),objLen,nullptr,0,0),"createHash");
    checkBCrypt(BCryptHashData(hh.h,(PUCHAR)data,(ULONG)len,0),"hashData");
    checkBCrypt(BCryptFinishHash(hh.h,hash,PATH_HASH_SIZE,0),"finish");
#elif defined(CAESURA_CRYPTO_OPENSSL)
    unsigned int olen = PATH_HASH_SIZE;
    EVP_MD_CTX* md = EVP_MD_CTX_new();
    if(md) { EVP_DigestInit_ex(md,EVP_sha256(),nullptr); EVP_DigestUpdate(md,data,len); EVP_DigestFinal_ex(md,hash,&olen); EVP_MD_CTX_free(md); }
    else memset(hash,0,PATH_HASH_SIZE);
#else
    memset(hash,0,PATH_HASH_SIZE);
#endif
}

// Ed25519 (portable)
bool CryptoEngine::sign(const uint8_t* data, size_t len, const uint8_t pk[64], uint8_t sig[SIGNATURE_SIZE])
{ ed25519_sign(sig,data,len,pk+32,pk); return true; }
bool CryptoEngine::verify(const uint8_t* data, size_t len, const uint8_t pub[PUBLICKEY_SIZE], const uint8_t sig[SIGNATURE_SIZE])
{ return ed25519_verify(sig,data,len,pub)!=0; }

// RNG
static void rng_fallback(uint8_t* buf, size_t n) {
    FILE* f = fopen("/dev/urandom","rb");
    if(!f || fread(buf,1,n,f)!=n) throw std::runtime_error("RNG failed");
    if(f) fclose(f);
}
void CryptoEngine::generateKey(uint8_t key[AES_KEY_SIZE]) {
#if defined(_WIN32)
    if(!BCRYPT_SUCCESS(BCryptGenRandom(nullptr,key,32,BCRYPT_USE_SYSTEM_PREFERRED_RNG))) throw std::runtime_error("BCryptGenRandom failed");
#elif defined(CAESURA_CRYPTO_OPENSSL)
    if(RAND_bytes(key,32)!=1) throw std::runtime_error("RAND_bytes failed");
#else
    rng_fallback(key,32);
#endif
}
void CryptoEngine::generateNonce(uint8_t nonce[AES_NONCE_SIZE]) {
#if defined(_WIN32)
    if(!BCRYPT_SUCCESS(BCryptGenRandom(nullptr,nonce,12,BCRYPT_USE_SYSTEM_PREFERRED_RNG))) throw std::runtime_error("BCryptGenRandom failed");
#elif defined(CAESURA_CRYPTO_OPENSSL)
    if(RAND_bytes(nonce,12)!=1) throw std::runtime_error("RAND_bytes failed");
#else
    rng_fallback(nonce,12);
#endif
}
void CryptoEngine::generateKeyPair(uint8_t pub[PUBLICKEY_SIZE], uint8_t pk[64])
{ uint8_t seed[32]; generateKey(seed); ed25519_create_keypair(pub,pk,seed); }

// Key file I/O (standard C)
bool CryptoEngine::readPublicKey(const std::string& path, uint8_t key[PUBLICKEY_SIZE])
{ FILE* f=fopen(path.c_str(),"rb"); if(!f) return false; bool ok=fread(key,1,32,f)==32; fclose(f); return ok; }
bool CryptoEngine::readPrivateKey(const std::string& path, uint8_t key[64])
{ FILE* f=fopen(path.c_str(),"rb"); if(!f) return false; bool ok=fread(key,1,64,f)==64; fclose(f); return ok; }
bool CryptoEngine::writePublicKey(const std::string& path, const uint8_t key[PUBLICKEY_SIZE])
{ FILE* f=fopen(path.c_str(),"wb"); if(!f) return false; bool ok=fwrite(key,1,32,f)==32; fclose(f); return ok; }
bool CryptoEngine::writePrivateKey(const std::string& path, const uint8_t key[64])
{ FILE* f=fopen(path.c_str(),"wb"); if(!f) return false; bool ok=fwrite(key,1,64,f)==64; fclose(f); return ok; }

} // namespace caesura::carc
