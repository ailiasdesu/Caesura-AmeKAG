// test_archive.cpp - Archive module tests (CARC, CryptoEngine, interfaces)
#include "doctest.h"
#include "archive/CARCReader.h"
#include "archive/CARCWriter.h"
#include "archive/CryptoEngine.h"
#include "archive/api/IArchiveReader.h"
#include "archive/api/IArchiveWriter.h"
#include "archive/api/ICryptoEngine.h"
#include "di/BackendRegistry.h"
#include <cstdio>
#include <cstring>

using namespace Caesura;

static void ensureCryptoRegistered() {
    static bool done = false;
    if (!done) {
        BackendRegistry::instance().setCryptoEngine(&carc::CryptoEngine::instance());
        done = true;
    }
}

TEST_CASE("CARCReader::open invalid file returns false") {
    carc::CARCReader reader;
    CHECK(reader.open("nonexistent.carc") == false);
    CHECK(reader.isOpen() == false);
}

TEST_CASE("ICryptoEngine encrypt/decrypt round-trip") {
    ensureCryptoRegistered();
    uint8_t key[32], nonce[12], tag[16];
    carc::CryptoEngine::generateKey(key);
    carc::CryptoEngine::generateNonce(nonce);
    const char* msg = "The quick brown fox jumps over the lazy dog";
    auto encrypted = carc::CryptoEngine::encrypt(
        reinterpret_cast<const uint8_t*>(msg), strlen(msg), key, nonce, tag);
    CHECK(encrypted.size() > 0);
    auto decrypted = carc::CryptoEngine::decrypt(
        encrypted.data(), encrypted.size(), key, nonce, tag);
    CHECK(decrypted.size() == strlen(msg));
    CHECK(std::memcmp(decrypted.data(), msg, strlen(msg)) == 0);
}

TEST_CASE("ICryptoEngine SHA-256 produces known hash") {
    ensureCryptoRegistered();
    const char* msg = "hello";
    uint8_t hash[32];
    carc::CryptoEngine::sha256(reinterpret_cast<const uint8_t*>(msg), 5, hash);
    uint8_t expected[32] = {
        0x2c,0xf2,0x4d,0xba,0x5f,0xb0,0xa3,0x0e,
        0x26,0xe8,0x3b,0x2a,0xc5,0xb9,0xe2,0x9e,
        0x1b,0x16,0x1e,0x5c,0x1f,0xa7,0x42,0x5e,
        0x73,0x04,0x33,0x62,0x93,0x8b,0x98,0x24
    };
    CHECK(std::memcmp(hash, expected, 32) == 0);
}

TEST_CASE("ICryptoEngine key generation produces non-zero keys") {
    ensureCryptoRegistered();
    uint8_t publicKey[32] = {}, privateKey[64] = {};
    carc::CryptoEngine::generateKeyPair(publicKey, privateKey);
    bool pubNonZero = false, privNonZero = false;
    for (int i = 0; i < 32; i++) if (publicKey[i] != 0) pubNonZero = true;
    for (int i = 0; i < 64; i++) if (privateKey[i] != 0) privNonZero = true;
    CHECK(pubNonZero);
    CHECK(privNonZero);
}

TEST_CASE("IArchiveReader interface completeness check") {
    carc::CARCReader reader;
    carc::IArchiveReader* iface = &reader;
    CHECK(iface->isOpen() == false);
    CHECK(iface->numFiles() == 0);
    CHECK(iface->hasFile("nonexistent") == false);
}

TEST_CASE("IArchiveWriter interface completeness check") {
    carc::CARCWriter writer;
    carc::IArchiveWriter* iface = &writer;
    CHECK(iface->create("") == false);
}

TEST_CASE("BackendRegistry::getCryptoEngine returns registered engine") {
    ensureCryptoRegistered();
    auto* crypto = BackendRegistry::instance().getCryptoEngine();
    CHECK(crypto != nullptr);
}

TEST_CASE("ICryptoEngine interface through BackendRegistry") {
    ensureCryptoRegistered();
    auto* iface = BackendRegistry::instance().getCryptoEngine();
    REQUIRE(iface != nullptr);
    uint8_t key[32], nonce[12], tag[16];
    iface->generateKey(key, sizeof(key));
    iface->generateNonce(nonce, sizeof(nonce));
    const char* msg = "interface test";
    auto enc = iface->encrypt(
        reinterpret_cast<const uint8_t*>(msg), strlen(msg),
        key, sizeof(key), nonce, sizeof(nonce), tag, sizeof(tag));
    CHECK(enc.size() > 0);
    auto dec = iface->decrypt(
        enc.data(), enc.size(),
        key, sizeof(key), nonce, sizeof(nonce), tag, sizeof(tag));
    CHECK(std::memcmp(dec.data(), msg, strlen(msg)) == 0);
}

TEST_CASE("CARCReader::close on unopened reader is safe") {
    carc::CARCReader reader;
    reader.close();
}

TEST_CASE("CARCWriter::addFile before create returns false") {
    carc::CARCWriter writer;
    CHECK(writer.addFile("test.txt", reinterpret_cast<const uint8_t*>("data"), 4) == false);
}

TEST_CASE("CARCWriter::finalize before create returns false") {
    carc::CARCWriter writer;
    CHECK(writer.finalize() == false);
}
