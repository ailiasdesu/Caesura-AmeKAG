// test_archive.cpp - Archive module tests (CARC, CryptoEngine, interfaces)
#include "doctest.h"
#include "archive/CARCReader.h"
#include "archive/CARCWriter.h"
#include "archive/CryptoEngine.h"
#include "archive/api/IArchiveReader.h"
#include "archive/api/IArchiveWriter.h"
#include "archive/api/ICryptoEngine.h"
#include "di/BackendRegistry.h"
#include "TestPaths.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace Caesura;

namespace {

class ScopedCryptoRegistration {
public:
    ScopedCryptoRegistration()
        : m_previous(BackendRegistry::instance().getCryptoEngine()) {
        BackendRegistry::instance().setCryptoEngine(&m_engine);
    }

    ~ScopedCryptoRegistration() {
        BackendRegistry::instance().setCryptoEngine(m_previous);
    }

    carc::CryptoEngine* engine() { return &m_engine; }

private:
    carc::CryptoEngine m_engine;
    carc::ICryptoEngine* m_previous;
};

} // namespace

TEST_CASE("CARCReader::open invalid file returns false") {
    carc::CARCReader reader;
    CHECK(reader.open("nonexistent.carc") == false);
    CHECK(reader.isOpen() == false);
}

TEST_CASE("ICryptoEngine encrypt/decrypt round-trip") {
    ScopedCryptoRegistration cryptoRegistration;
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
    ScopedCryptoRegistration cryptoRegistration;
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
    ScopedCryptoRegistration cryptoRegistration;
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
    auto& registry = BackendRegistry::instance();
    auto* previous = registry.getCryptoEngine();
    {
        ScopedCryptoRegistration cryptoRegistration;
        CHECK(registry.getCryptoEngine() == cryptoRegistration.engine());
    }
    CHECK(registry.getCryptoEngine() == previous);
}

TEST_CASE("ICryptoEngine interface through BackendRegistry") {
    ScopedCryptoRegistration cryptoRegistration;
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

TEST_CASE("Crypto: key-handle cache switches keys correctly") {
    // The thread-local BCrypt handle cache must invalidate when the key
    // changes; interleave two keys and verify round trips stay exact.
    Caesura::carc::CryptoEngine crypto;
    const std::string a = "key-A-key-A-key-A-key-A-key-A-key-A-key-A-key-A";  // 32B
    const std::string b = "key-B-key-B-key-B-key-B-key-B-key-B-key-B-key-B";
    const std::string msg = "the quick brown fox jumps over the lazy dog";
    for (int i = 0; i < 8; ++i) {
        const auto& key = (i % 2 == 0) ? a : b;
        uint8_t nonce[12], tag[16];
        crypto.generateNonce(nonce, sizeof(nonce));
        auto ct = crypto.encrypt(
            reinterpret_cast<const uint8_t*>(msg.data()), msg.size(),
            reinterpret_cast<const uint8_t*>(key.data()), 32,
            nonce, sizeof(nonce), tag, sizeof(tag));
        REQUIRE_FALSE(ct.empty());
        auto pt = crypto.decrypt(ct.data(), ct.size(),
            reinterpret_cast<const uint8_t*>(key.data()), 32,
            nonce, sizeof(nonce), tag, sizeof(tag));
        CHECK(std::string(reinterpret_cast<char*>(pt.data()), pt.size()) == msg);
    }
}

// =============================================================================
// G10: CARC archive module boundary tests (write/read roundtrip, tamper,
//      key separation, signature, large/streaming read, empty archive, paths)
// =============================================================================

namespace {

// Helper: build an archive in a fresh ScopedTempDir and return the .carc path.
// Files is a vector of {relativePath, raw bytes}.
std::filesystem::path buildTestArchive(
    const std::filesystem::path& dir,
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files,
    const std::string& name = "out.carc")
{
    namespace fs = std::filesystem;
    const fs::path arc = dir / name;
    {
        Caesura::carc::CARCWriter writer;
        REQUIRE(writer.create(arc.string()));
        for (const auto& [rel, data] : files) {
            REQUIRE(writer.addFile(rel, data.data(), data.size()));
        }
        REQUIRE(writer.finalize());
    }
    REQUIRE(fs::exists(arc));
    REQUIRE(fs::file_size(arc) > 0);
    return arc;
}

// Flip the byte at byteIndex in the given file (in place).
void flipByte(const std::filesystem::path& path, std::streamoff byteIndex)
{
    namespace fs = std::filesystem;
    std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
    REQUIRE(f.is_open());
    f.seekg(byteIndex, std::ios::beg);
    char c = 0;
    REQUIRE(f.read(&c, 1));
    c = static_cast<char>(static_cast<unsigned char>(c) ^ 0x01); // actually tamper
    REQUIRE(f.seekp(byteIndex, std::ios::beg));
    f.write(&c, 1);
    REQUIRE(f.good());
}

} // namespace

TEST_CASE("CARC G10: multi-file write/read roundtrip (binary + text + nested)") {
    namespace fs = std::filesystem;
    Caesura::TestPaths::ScopedTempDir temp("archive_g10_roundtrip");
    const fs::path arc = temp.path() / "multi.carc";

    // Text file + nested text + binary file with null bytes and high bytes.
    const std::string textA = "plain text payload line 1\nline 2";
    const std::string nested = "nested/dir/deep.txt";
    std::vector<uint8_t> bin;
    for (int i = 0; i < 256; ++i) bin.push_back(static_cast<uint8_t>(i));
    for (int i = 0; i < 300; ++i) bin.push_back(static_cast<uint8_t>(0)); // nulls
    bin.push_back(0xFF); bin.push_back(0x00); bin.push_back(0x80);

    {
        Caesura::carc::CARCWriter writer;
        REQUIRE(writer.create(arc.string()));
        REQUIRE(writer.addFile("text.txt",
            reinterpret_cast<const uint8_t*>(textA.data()), textA.size()));
        REQUIRE(writer.addFile(nested,
            reinterpret_cast<const uint8_t*>("deep content"), 13));
        REQUIRE(writer.addFile("bin/data.bin", bin.data(), bin.size()));
        REQUIRE(writer.finalize());
    }

    Caesura::carc::CARCReader reader;
    REQUIRE(reader.open(arc.string()));
    CHECK(reader.numFiles() == 3);

    // Order-independent existence checks.
    CHECK(reader.hasFile("text.txt"));
    CHECK(reader.hasFile(nested));
    CHECK(reader.hasFile("bin/data.bin"));

    auto gotText = reader.readFile("text.txt");
    REQUIRE_FALSE(gotText.empty());
    CHECK(gotText.size() == textA.size());
    CHECK(std::memcmp(gotText.data(), textA.data(), textA.size()) == 0);

    auto gotNested = reader.readFile(nested);
    REQUIRE_FALSE(gotNested.empty());
    CHECK(gotNested.size() == 13);
    CHECK(std::memcmp(gotNested.data(), "deep content", 13) == 0);

    auto gotBin = reader.readFile("bin/data.bin");
    REQUIRE_FALSE(gotBin.empty());
    CHECK(gotBin.size() == bin.size());
    CHECK(std::memcmp(gotBin.data(), bin.data(), bin.size()) == 0);
}

TEST_CASE("CARC G10: body tamper (flip byte in content block) fails to open") {
    namespace fs = std::filesystem;
    Caesura::TestPaths::ScopedTempDir temp("archive_g10_bodytamper");
    const std::string payload = "the signed content body of the archive";
    const fs::path arc = buildTestArchive(temp.path(),
        {{"data.txt", std::vector<uint8_t>(payload.begin(), payload.end())}});

    // The content block starts right after the 64-byte header.
    flipByte(arc, static_cast<std::streamoff>(sizeof(Caesura::carc::CARCHeader)) + 8);

    // Signature covers header + content + index, so a body flip must fail open.
    Caesura::carc::CARCReader reader;
    CHECK_FALSE(reader.open(arc.string()));
    CHECK_FALSE(reader.isOpen());
}

TEST_CASE("CARC G10: header tamper (flip byte in header) fails to open") {
    namespace fs = std::filesystem;
    Caesura::TestPaths::ScopedTempDir temp("archive_g10_headertamper");
    const fs::path arc = buildTestArchive(temp.path(),
        {{"data.txt", std::vector<uint8_t>({'h', 'e', 'l', 'l', 'o'})}});

    // Flip a byte inside the 64-byte header (e.g. reserved field at offset 48).
    flipByte(arc, 48);

    Caesura::carc::CARCReader reader;
    CHECK_FALSE(reader.open(arc.string()));
    CHECK_FALSE(reader.isOpen());
}

TEST_CASE("CARC G10: archive written with key A fails to open with key B") {
    namespace fs = std::filesystem;
    Caesura::TestPaths::ScopedTempDir temp("archive_g10_keysep");
    const fs::path arc = temp.path() / "keyed.carc";
    const fs::path pubA = temp.path() / "keyA.pub";
    const fs::path privA = temp.path() / "keyA.priv";

    // Archive A is created with an explicit key pair (public key = key A).
    {
        Caesura::carc::CARCWriter writer;
        REQUIRE(writer.create(arc.string(), privA.string(), pubA.string()));
        REQUIRE(writer.addFile("secret.txt",
            reinterpret_cast<const uint8_t*>("classified"), 10));
        REQUIRE(writer.finalize());
    }

    // Opening with the archive's own public key (key A) succeeds.
    {
        Caesura::carc::CARCReader reader;
        REQUIRE(reader.open(arc.string(), pubA.string()));
        CHECK(reader.hasFile("secret.txt"));
    }

    // A different keypair B must not be able to open/open the archive.
    const fs::path pubB = temp.path() / "keyB.pub";
    const fs::path privB = temp.path() / "keyB.priv";
    {
        Caesura::carc::CARCWriter probe;   // generates an unrelated keypair
        REQUIRE(probe.create(temp.path().string() + "/keyBprobe.carc",
                             privB.string(), pubB.string()));
        REQUIRE(probe.addFile("x.txt", reinterpret_cast<const uint8_t*>("x"), 1));
        REQUIRE(probe.finalize());
    }

    // Reading with public key B must fail (signature / index decrypt fail).
    {
        Caesura::carc::CARCReader readerB;
        CHECK_FALSE(readerB.open(arc.string(), pubB.string()));
        CHECK_FALSE(readerB.isOpen());
    }
}

TEST_CASE("CARC G10: Ed25519 verify passes for valid, fails for tampered/mismatched") {
    uint8_t pub[Caesura::carc::PUBLICKEY_SIZE];
    uint8_t priv[64];
    Caesura::carc::CryptoEngine::generateKeyPair(pub, priv);

    std::string msg = "signature verification boundary payload";
    uint8_t sig[Caesura::carc::SIGNATURE_SIZE];
    REQUIRE(Caesura::carc::CryptoEngine::sign(
        reinterpret_cast<const uint8_t*>(msg.data()), msg.size(), priv, sig));

    // Valid signature verifies with the correct public key.
    CHECK(Caesura::carc::CryptoEngine::verify(
        reinterpret_cast<const uint8_t*>(msg.data()), msg.size(), pub, sig));

    // Tampered message must fail.
    std::string tampered = msg;
    tampered[3] ^= 0x01;
    CHECK_FALSE(Caesura::carc::CryptoEngine::verify(
        reinterpret_cast<const uint8_t*>(tampered.data()), tampered.size(), pub, sig));

    // Mismatched signature (flipped bit) must fail even on the original message.
    uint8_t badSig[Caesura::carc::SIGNATURE_SIZE];
    std::memcpy(badSig, sig, sizeof(badSig));
    badSig[0] ^= 0x01;
    CHECK_FALSE(Caesura::carc::CryptoEngine::verify(
        reinterpret_cast<const uint8_t*>(msg.data()), msg.size(), pub, badSig));

    // Signature from a different keypair must not verify.
    uint8_t otherPub[Caesura::carc::PUBLICKEY_SIZE];
    uint8_t otherPriv[64];
    Caesura::carc::CryptoEngine::generateKeyPair(otherPub, otherPriv);
    CHECK_FALSE(Caesura::carc::CryptoEngine::verify(
        reinterpret_cast<const uint8_t*>(msg.data()), msg.size(), otherPub, sig));
}

TEST_CASE("CARC G10: large file streaming/chunked read returns exact bytes") {
    namespace fs = std::filesystem;
    Caesura::TestPaths::ScopedTempDir temp("archive_g10_large");
    const fs::path arc = temp.path() / "large.carc";

    // Incompressible-ish 2 MiB blob so the archive genuinely holds the bytes.
    constexpr size_t kLarge = 2u * 1024u * 1024u;
    std::vector<uint8_t> big(kLarge);
    std::srand(42);
    for (size_t i = 0; i < big.size(); ++i) big[i] = static_cast<uint8_t>(std::rand() & 0xFF);

    {
        Caesura::carc::CARCWriter writer;
        REQUIRE(writer.create(arc.string()));
        REQUIRE(writer.addFile("big.bin", big.data(), big.size()));
        REQUIRE(writer.finalize());
    }

    Caesura::carc::CARCReader reader;
    REQUIRE(reader.open(arc.string()));
    CHECK(reader.hasFile("big.bin"));

    // Full read returns the exact backing bytes.
    auto got = reader.readFile("big.bin");
    REQUIRE_FALSE(got.empty());
    CHECK(got.size() == kLarge);

    // Re-read and consume in fixed-size chunks; every chunk must equal source.
    auto reread = reader.readFile("big.bin");
    REQUIRE(reread.size() == kLarge);
    const size_t chunk = 65536u;
    bool match = true;
    for (size_t off = 0; off < reread.size(); off += chunk) {
        const size_t n = std::min(chunk, reread.size() - off);
        if (std::memcmp(reread.data() + off, big.data() + off, n) != 0) { match = false; break; }
    }
    CHECK(match);
    CHECK(std::memcmp(got.data(), big.data(), big.size()) == 0);
}

TEST_CASE("CARC G10: empty archive (zero files) cannot be written via API") {
    namespace fs = std::filesystem;
    // Engine boundary check: the writer refuses to finalize a zero-file archive.
    // This documents actual behavior -- an empty CARC cannot be produced with
    // the current IArchiveWriter contract.
    Caesura::TestPaths::ScopedTempDir temp("archive_g10_empty");
    const fs::path arc = temp.path() / "empty.carc";
    {
        Caesura::carc::CARCWriter writer;
        REQUIRE(writer.create(arc.string()));
        CHECK_FALSE(writer.finalize());   // finalize() returns false with no files
    }
    // The output stream is closed without an archive; no valid file is emitted.
    bool producesValidArchive = fs::exists(arc) && fs::file_size(arc) > 0;
    CHECK_FALSE(producesValidArchive);
}

TEST_CASE("CARC G10: directory-style paths require exact spelling (no normalization)") {
    namespace fs = std::filesystem;
    Caesura::TestPaths::ScopedTempDir temp("archive_g10_paths");
    const fs::path arc = buildTestArchive(temp.path(),
        {{"dir/sub/leaf.txt", std::vector<uint8_t>({'l','e','a','f'})}});

    Caesura::carc::CARCReader reader;
    REQUIRE(reader.open(arc.string()));

    // The exact spelling used at write time round-trips.
    CHECK(reader.hasFile("dir/sub/leaf.txt"));
    auto exact = reader.readFile("dir/sub/leaf.txt");
    REQUIRE_FALSE(exact.empty());
    CHECK(std::memcmp(exact.data(), "leaf", 4) == 0);

    // CARC keys by exact byte string -- differing separator spellings are
    // distinct entries (behavioral documentation, no normalization step).
    CHECK_FALSE(reader.hasFile("dir\\sub\\leaf.txt"));   // backslashes
    CHECK_FALSE(reader.hasFile("./dir/sub/leaf.txt"));       // leading ./
    CHECK_FALSE(reader.hasFile("dir//sub//leaf.txt"));       // doubled slashes
    CHECK(reader.readFile("dir\\sub\\leaf.txt").empty());
    CHECK(reader.readFile("DIR/sub/leaf.txt").empty());      // case-sensitive
}
