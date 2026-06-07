#include "doctest.h"
#include "Carc/CARCFormat.h"
#include "Carc/CryptoEngine.h"
#include "Carc/CARCWriter.h"
#include "Carc/CARCReader.h"
#include "Carc/CRLManager.h"
#include <cstring>
#include <cstdio>
#include <vector>
#include <fstream>
#include <filesystem>

using namespace caesura::carc;

TEST_CASE("CARCFormat::constants") {
    CHECK(CARC_MAGIC == 0x43524143);
    CHECK(CARC_VERSION == 1);
    CHECK(sizeof(CARCHeader) == 64);
    CHECK(AES_KEY_SIZE == 32);
    CHECK(AES_NONCE_SIZE == 12);
    CHECK(AES_TAG_SIZE == 16);
}

TEST_CASE("CryptoEngine::AES-256-GCM round-trip") {
    uint8_t key[AES_KEY_SIZE], nonce[AES_NONCE_SIZE], tag[AES_TAG_SIZE];
    for (int i = 0; i < AES_KEY_SIZE; i++) key[i] = (uint8_t)i;
    for (int i = 0; i < AES_NONCE_SIZE; i++) nonce[i] = (uint8_t)(i + 0x42);
    auto ct = CryptoEngine::encrypt((const uint8_t*)"Hello CARC!", 11, key, nonce, tag);
    REQUIRE_FALSE(ct.empty());
    auto pt = CryptoEngine::decrypt(ct.data(), ct.size(), key, nonce, tag);
    REQUIRE_FALSE(pt.empty());
    CHECK(memcmp(pt.data(), "Hello CARC!", 11) == 0);
}

TEST_CASE("CryptoEngine::bad key fails decrypt") {
    uint8_t key[AES_KEY_SIZE], bad[AES_KEY_SIZE];
    uint8_t nonce[AES_NONCE_SIZE], tag[AES_TAG_SIZE];
    for (int i = 0; i < AES_KEY_SIZE; i++) { key[i] = (uint8_t)i; bad[i] = (uint8_t)(i ^ 0xFF); }
    for (int i = 0; i < AES_NONCE_SIZE; i++) nonce[i] = (uint8_t)i;
    auto ct = CryptoEngine::encrypt((const uint8_t*)"test", 4, key, nonce, tag);
    auto pt = CryptoEngine::decrypt(ct.data(), ct.size(), bad, nonce, tag);
    CHECK((pt.empty() || memcmp(pt.data(), "test", 4) != 0));
}

TEST_CASE("CryptoEngine::generateKey entropy") {
    uint8_t k1[AES_KEY_SIZE], k2[AES_KEY_SIZE];
    CryptoEngine::generateKey(k1);
    CryptoEngine::generateKey(k2);
    CHECK(memcmp(k1, k2, AES_KEY_SIZE) != 0);
}

TEST_CASE("CryptoEngine::generateNonce entropy") {
    uint8_t n1[AES_NONCE_SIZE], n2[AES_NONCE_SIZE];
    CryptoEngine::generateNonce(n1);
    CryptoEngine::generateNonce(n2);
    CHECK(memcmp(n1, n2, AES_NONCE_SIZE) != 0);
}

TEST_CASE("CryptoEngine::keypair generation") {
    uint8_t pub[PUBLICKEY_SIZE], priv[64];
    CryptoEngine::generateKeyPair(pub, priv);
    bool ok = false;
    for (int i = 0; i < PUBLICKEY_SIZE; i++) if (pub[i] != 0) ok = true;
    CHECK(ok);
}

TEST_CASE("CryptoEngine::SHA-256") {
    uint8_t hash[PATH_HASH_SIZE];
    CryptoEngine::sha256((const uint8_t*)"test", 4, hash);
    bool ok = false;
    for (int i = 0; i < PATH_HASH_SIZE; i++) if (hash[i] != 0) ok = true;
    CHECK(ok);
}

TEST_CASE("CRLManager::lifecycle") {
    CRLManager crl;
    CHECK_FALSE(crl.isRevoked("x"));
    CHECK(crl.revokedCount() == 0);
    CHECK(crl.mode() == CRLMode::Hybrid);
    crl.addRevoked("fp");
    CHECK(crl.isRevoked("fp"));
    CHECK(crl.revokedCount() == 1);
    crl.clear();
    CHECK(crl.revokedCount() == 0);
}

TEST_CASE("CARC container: write then read") {
    namespace fs = std::filesystem;
    fs::remove("test_carc.carc");
    {
        CARCWriter w;
        CHECK(w.create("test_carc.carc"));
        CHECK(w.addFile("data.txt", (const uint8_t*)"payload", 7));
        CHECK(w.finalize());
    }
    CHECK(fs::exists("test_carc.carc"));
    CHECK(fs::file_size("test_carc.carc") > 0);
    {
        CARCReader r;
        if (r.open("test_carc.carc")) {
            CHECK(r.numFiles() == 1);
            CHECK(r.hasFile("data.txt"));
        }
    }
    fs::remove("test_carc.carc");
}