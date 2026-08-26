// test_archive.cpp - Archive module tests (CARC, CryptoEngine, interfaces)
#include "doctest.h"
#include "archive/CARCReader.h"
#include "archive/CARCWriter.h"
#include "archive/CarcAssetProvider.h"
#include "archive/CryptoEngine.h"
#include "archive/DeltaCARC.h"
#include "resource/ProviderChain.h"
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
// =============================================================================
// G11: Additional CARC / DeltaCARC / crypto / streaming boundary tests
//      (deep + special-character paths, empty files, duplicate keys, large
//       streaming, truncation/corruption handling, key-length crypto bounds,
//       DeltaCARC apply-on-tamper)
// =============================================================================

namespace {

// Build a CARC from {relativePath, text content} pairs.
std::filesystem::path buildTextCarc(
    const std::filesystem::path& dir,
    const std::vector<std::pair<std::string, std::string>>& files,
    const std::string& name = "g11.carc")
{
    namespace fs = std::filesystem;
    const fs::path arc = dir / name;
    {
        Caesura::carc::CARCWriter writer;
        REQUIRE(writer.create(arc.string()));
        for (const auto& [rel, content] : files) {
            REQUIRE(writer.addFile(rel,
                reinterpret_cast<const uint8_t*>(content.data()), content.size()));
        }
        REQUIRE(writer.finalize());
    }
    REQUIRE(fs::exists(arc));
    REQUIRE(fs::file_size(arc) > 0);
    return arc;
}

std::string readText(carc::CARCReader& r, const std::string& path) {
    auto data = r.readFile(path);
    return std::string(data.begin(), data.end());
}

} // namespace

// --- CARC format: special characters / deep nesting / empty files -----------

TEST_CASE("CARC G11: special-character and deeply-nested path keys round-trip") {
    namespace fs = std::filesystem;
    Caesura::TestPaths::ScopedTempDir temp("archive_g11_special");
    // CARC keys are byte-exact (SHA-256 of the path bytes), so arbitrary bytes
    // -- including UTF-8 sequences, spaces and quotes -- must round-trip.
    const std::string unicodeName = "\xc3\xa9\xe4\xb8\xad safe space .txt";   // "é中文 ..."
    const fs::path arc = buildTestArchive(temp.path(), {
        {"a/b/c/d/e/f/leaf.txt", std::vector<uint8_t>({'d','e','e','p'})},
        {"quoted\"with'quote.txt", std::vector<uint8_t>({'q'})},
        {unicodeName, std::vector<uint8_t>({'u','n'})},
    });

    Caesura::carc::CARCReader reader;
    REQUIRE(reader.open(arc.string()));
    CHECK(reader.numFiles() == 3);

    CHECK(reader.hasFile("a/b/c/d/e/f/leaf.txt"));
    CHECK(reader.hasFile("quoted\"with'quote.txt"));
    CHECK(reader.hasFile(unicodeName));

    auto leaf = reader.readFile("a/b/c/d/e/f/leaf.txt");
    REQUIRE_FALSE(leaf.empty());
    CHECK(std::memcmp(leaf.data(), "deep", 4) == 0);

    auto quoted = reader.readFile("quoted\"with'quote.txt");
    REQUIRE_FALSE(quoted.empty());
    CHECK(std::memcmp(quoted.data(), "q", 1) == 0);

    auto unicode = reader.readFile(unicodeName);
    REQUIRE_FALSE(unicode.empty());
    CHECK(std::memcmp(unicode.data(), "un", 2) == 0);
}

TEST_CASE("CARC G11: empty-content file entries round-trip and stay addressable") {
    namespace fs = std::filesystem;
    Caesura::TestPaths::ScopedTempDir temp("archive_g11_emptyfile");
    const fs::path arc = buildTestArchive(temp.path(), {
        {"zero.txt", std::vector<uint8_t>()},
        {"also/empty.txt", std::vector<uint8_t>()},
        {"sized.txt", std::vector<uint8_t>({'x'})},
    });

    Caesura::carc::CARCReader reader;
    REQUIRE(reader.open(arc.string()));
    CHECK(reader.numFiles() == 3);

    // Empty files remain in the index (addressable by hash) ...
    CHECK(reader.hasFile("zero.txt"));
    CHECK(reader.hasFile("also/empty.txt"));

    // ... and reading them yields an empty (but successful) payload.
    CHECK(reader.readFile("zero.txt").empty());
    CHECK(reader.readFile("also/empty.txt").empty());

    // Both empty entries carry originalSize == 0 in the index.
    int zeroSizeCount = 0;
    for (const auto& [hash, info] : reader.index()) {
        (void)hash;
        if (info.originalSize == 0) zeroSizeCount++;
    }
    CHECK(zeroSizeCount == 2);
}

TEST_CASE("CARC G11: duplicate path keys are deduplicated (update existing entry)") {
    namespace fs = std::filesystem;
    // CARCWriter::addFile is idempotent by relative path: re-adding an existing
    // path updates the pending entry (last write wins) instead of appending a
    // duplicate. The index therefore holds exactly one entry per unique path,
    // so numFiles() equals the unique-path count and readFile() returns the
    // last-added content -- consistent with the reader's hash-keyed map.
    Caesura::TestPaths::ScopedTempDir temp("archive_g11_dup");
    const fs::path arc = temp.path() / "dup.carc";
    {
        Caesura::carc::CARCWriter writer;
        REQUIRE(writer.create(arc.string()));
        REQUIRE(writer.addFile("same.txt", reinterpret_cast<const uint8_t*>("first"), 5));
        REQUIRE(writer.addFile("same.txt", reinterpret_cast<const uint8_t*>("second"), 6));
        REQUIRE(writer.addFile("other.txt", reinterpret_cast<const uint8_t*>("x"), 1));
        REQUIRE(writer.finalize());
    }

    Caesura::carc::CARCReader reader;
    REQUIRE(reader.open(arc.string()));
    CHECK(reader.hasFile("same.txt"));
    // One index entry per unique path -> numFiles() counts unique paths, not
    // raw addFile() calls.
    CHECK(reader.numFiles() == 2);
    // readFile() returns the last-added (overwriting) content.
    auto got = reader.readFile("same.txt");
    REQUIRE_FALSE(got.empty());
    CHECK(got.size() == 6);
    CHECK(std::memcmp(got.data(), "second", 6) == 0);
    // The distinct path was not collapsed by the dedup.
    CHECK(reader.hasFile("other.txt"));
}

// --- Streaming I/O: > 2 MiB round-trip / trunkated-file graceful failure -----

TEST_CASE("CARC G11: >2MiB incompressible payload streams back byte-exact") {
    namespace fs = std::filesystem;
    Caesura::TestPaths::ScopedTempDir temp("archive_g11_big");
    const fs::path arc = temp.path() / "big.carc";
    constexpr size_t kBig = 5u * 1024u * 1024u;  // strictly greater than 2 MiB
    std::vector<uint8_t> big(kBig);
    std::srand(1234);
    for (size_t i = 0; i < big.size(); ++i) big[i] = static_cast<uint8_t>(std::rand() & 0xFF);

    {
        Caesura::carc::CARCWriter writer;
        REQUIRE(writer.create(arc.string()));
        REQUIRE(writer.addFile("big.bin", big.data(), big.size()));
        REQUIRE(writer.finalize());
    }

    Caesura::carc::CARCReader reader;
    REQUIRE(reader.open(arc.string()));
    REQUIRE(reader.hasFile("big.bin"));

    auto got = reader.readFile("big.bin");
    REQUIRE(got.size() == kBig);

    // Segment-wise equality over the whole payload.
    const size_t seg = 262144u;  // 256 KiB
    bool match = true;
    for (size_t off = 0; off < kBig; off += seg) {
        const size_t n = std::min(seg, kBig - off);
        if (std::memcmp(got.data() + off, big.data() + off, n) != 0) { match = false; break; }
    }
    CHECK(match);
}

TEST_CASE("CARC G11: truncated archive fails open gracefully (never crashes)") {
    namespace fs = std::filesystem;
    Caesura::TestPaths::ScopedTempDir temp("archive_g11_trunc");
    const fs::path arc = buildTestArchive(temp.path(), {
        {"a.txt", std::vector<uint8_t>({'a','b','c','d'})},
        {"b.txt", std::vector<uint8_t>({'1','2','3'})},
    });
    const std::uintmax_t fullSize = fs::file_size(arc);
    REQUIRE(fullSize > sizeof(Caesura::carc::CARCHeader));

    // Probe: header-only, mid-file, and trailer-missing truncation points.
    const std::uintmax_t trailers = Caesura::carc::SIGNATURE_SIZE + Caesura::carc::PUBLICKEY_SIZE;
    const std::uintmax_t cuts[3] = {
        sizeof(Caesura::carc::CARCHeader),
        fullSize / 2,
        fullSize - trailers,
    };
    int idx = 0;
    for (const std::uintmax_t cut : cuts) {
        const fs::path t = temp.path() / ("trunc_" + std::to_string(idx++) + ".carc");
        {
            std::ifstream in(arc, std::ios::binary);
            std::ofstream out(t, std::ios::binary);
            std::vector<char> buf(static_cast<size_t>(cut));
            in.read(buf.data(), static_cast<std::streamsize>(cut));
            out.write(buf.data(), in.gcount());
        }
        Caesura::carc::CARCReader reader;
        auto opened = reader.open(t.string());
        // The reader must give a clear failure (no crash, no partial state).
        CAPTURE(cut);
        if (cut >= sizeof(Caesura::carc::CARCHeader)) {
            // Header reads fine but later structural checks fail.
            CHECK_FALSE(opened);
            CHECK_FALSE(reader.isOpen());
        } else {
            // Even a sub-header read must fail closed.
            CHECK_FALSE(opened);
        }
    }
}

// --- Read/write interaction: corrupted header / wrong magic / immediate read --

TEST_CASE("CARC G11: wrong magic and corrupted length fields reject open") {
    namespace fs = std::filesystem;
    Caesura::TestPaths::ScopedTempDir temp("archive_g11_corrupt");
    const fs::path baseArc = buildTestArchive(temp.path(),
        {{"a.txt", std::vector<uint8_t>({'x'})}});

    // Wrong magic: byte 0 flips 0x43('C') -> 0x42('B').
    {
        const fs::path bad = temp.path() / "badmagic.carc";
        fs::copy_file(baseArc, bad, fs::copy_options::overwrite_existing);
        flipByte(bad, 0);
        Caesura::carc::CARCReader reader;
        CHECK_FALSE(reader.open(bad.string()));
        CHECK_FALSE(reader.isOpen());
    }

    // Corrupt the 8-byte contentSize in the header (offset 16) to a huge value.
    {
        const fs::path bad = temp.path() / "badlen.carc";
        fs::copy_file(baseArc, bad, fs::copy_options::overwrite_existing);
        {
            std::fstream f(bad, std::ios::binary | std::ios::in | std::ios::out);
            const std::uint64_t huge = 0xFFFFFFFFFFFFFFFFull;
            f.seekp(16);
            f.write(reinterpret_cast<const char*>(&huge), 8);
        }
        Caesura::carc::CARCReader reader;
        CHECK_FALSE(reader.open(bad.string()));
        CHECK_FALSE(reader.isOpen());
    }

    // Corrupt the numFiles field (offset 40) so the index walk must abort.
    {
        const fs::path bad = temp.path() / "badcount.carc";
        fs::copy_file(baseArc, bad, fs::copy_options::overwrite_existing);
        {
            std::fstream f(bad, std::ios::binary | std::ios::in | std::ios::out);
            const std::uint32_t hugeCount = 0xFFFFFFF0u;
            f.seekp(40);
            f.write(reinterpret_cast<const char*>(&hugeCount), 4);
        }
        Caesura::carc::CARCReader reader;
        CHECK_FALSE(reader.open(bad.string()));
        CHECK_FALSE(reader.isOpen());
    }
}

TEST_CASE("CARC G11: write then immediately read back in the same session") {
    namespace fs = std::filesystem;
    Caesura::TestPaths::ScopedTempDir temp("archive_g11_immediate");
    const fs::path arc = temp.path() / "immediate.carc";
    const std::string text = "write-then-read boundary payload";
    {
        Caesura::carc::CARCWriter writer;
        REQUIRE(writer.create(arc.string()));
        REQUIRE(writer.addFile("note.txt",
            reinterpret_cast<const uint8_t*>(text.data()), text.size()));
        REQUIRE(writer.addFile("other/note.txt",
            reinterpret_cast<const uint8_t*>("other content"), 13));
        REQUIRE(writer.finalize());
    }

    // Open and read immediately after the writer finalizes (same test scope).
    Caesura::carc::CARCReader reader;
    REQUIRE(reader.open(arc.string()));
    REQUIRE(reader.hasFile("note.txt"));
    auto got = reader.readFile("note.txt");
    REQUIRE_FALSE(got.empty());
    CHECK(got.size() == text.size());
    CHECK(std::memcmp(got.data(), text.data(), text.size()) == 0);
    CHECK(readText(reader, "other/note.txt") == "other content");
}

// --- Encryption boundaries: key length / empty plaintext / multi-round -------

TEST_CASE("Crypto G11: non-256-bit key lengths are rejected (empty output)") {
    Caesura::carc::CryptoEngine crypto;
    const uint8_t msg[8] = {'a','b','c','d','e','f','g','h'};
    uint8_t nonce[12] = {}, tag[16] = {};

    uint8_t shortKey[16] = {};   // 128-bit: below the AES-256 minimum.
    auto enc16 = crypto.encrypt(msg, sizeof(msg), shortKey, sizeof(shortKey),
                                nonce, sizeof(nonce), tag, sizeof(tag));
    CHECK(enc16.empty());

    uint8_t tinyKey[8] = {};
    auto enc8 = crypto.encrypt(msg, sizeof(msg), tinyKey, sizeof(tinyKey),
                               nonce, sizeof(nonce), tag, sizeof(tag));
    CHECK(enc8.empty());

    // An oversized key (48 bytes) is rejected: AES-256 key length is exactly
    // AES_KEY_SIZE. A wrong key length is a configuration bug and must fail
    // fast (both encrypt and decrypt), symmetric with the too-short guard.
    uint8_t longKey[48] = {};
    for (int i = 0; i < 48; ++i) longKey[i] = static_cast<uint8_t>(i);
    auto enc48 = crypto.encrypt(msg, sizeof(msg), longKey, sizeof(longKey),
                                nonce, sizeof(nonce), tag, sizeof(tag));
    CHECK(enc48.empty());
    // Decrypt of the (never produced) oversized-key ciphertext also rejects.
    auto dec48 = crypto.decrypt(msg, sizeof(msg), longKey, sizeof(longKey),
                                nonce, sizeof(nonce), tag, sizeof(tag));
    CHECK(dec48.empty());
}

TEST_CASE("Crypto G11: empty plaintext rejected; wrong-key decrypt fails") {
    Caesura::carc::CryptoEngine crypto;
    uint8_t nonce[12] = {}, tag[16] = {};
    uint8_t key[32] = {0x11,0x22,0x33,0x44};

    // Empty plaintext (len 0) must not produce ciphertext on encrypt.
    auto emptyEnc = crypto.encrypt(key, 0, key, sizeof(key),
                                   nonce, sizeof(nonce), tag, sizeof(tag));
    CHECK(emptyEnc.empty());

    const char* msg = "sensitive data";
    uint8_t realKey[32] = {};
    std::memset(realKey, 0xAB, 32);
    uint8_t realNonce[12], realTag[16];
    crypto.generateNonce(realNonce, sizeof(realNonce));
    auto ct = crypto.encrypt(reinterpret_cast<const uint8_t*>(msg), std::strlen(msg),
                             realKey, sizeof(realKey),
                             realNonce, sizeof(realNonce), realTag, sizeof(realTag));
    REQUIRE_FALSE(ct.empty());

    // Decrypt with a different key (same nonce/tag) -> GCM auth fails -> empty.
    uint8_t wrongKey[32] = {};
    std::memset(wrongKey, 0xCD, 32);
    auto bad = crypto.decrypt(ct.data(), ct.size(), wrongKey, sizeof(wrongKey),
                              realNonce, sizeof(realNonce), realTag, sizeof(realTag));
    CHECK(bad.empty());
}

TEST_CASE("Crypto G11: three-round round-trip with isolated keys is exact") {
    Caesura::carc::CryptoEngine crypto;
    const char* msgs[3] = { "round one payload", "round two payload ....", "round three" };
    uint8_t keys[3][32];
    for (int r = 0; r < 3; ++r)
        for (int j = 0; j < 32; ++j)
            keys[r][j] = static_cast<uint8_t>((r + j) * 7 + 1);

    for (int r = 0; r < 3; ++r) {
        uint8_t nonce[12], tag[16];
        crypto.generateNonce(nonce, sizeof(nonce));
        auto ct = crypto.encrypt(reinterpret_cast<const uint8_t*>(msgs[r]), std::strlen(msgs[r]),
                                 keys[r], sizeof(keys[r]),
                                 nonce, sizeof(nonce), tag, sizeof(tag));
        REQUIRE_FALSE(ct.empty());
        auto pt = crypto.decrypt(ct.data(), ct.size(), keys[r], sizeof(keys[r]),
                                 nonce, sizeof(nonce), tag, sizeof(tag));
        REQUIRE_FALSE(pt.empty());
        CHECK(pt.size() == std::strlen(msgs[r]));
        CHECK(std::memcmp(pt.data(), msgs[r], pt.size()) == 0);
    }
}


// --- Nonce-reuse detection registry (round 88 / round 104 adjudication) ---

TEST_CASE("Crypto: nonce reuse under the same key is rejected, fresh nonce passes") {
    // The registry is process-scoped and on by default. Ensure it is active.
    Caesura::carc::CryptoEngine::setNonceReuseDetection(true);

    Caesura::carc::CryptoEngine crypto;
    uint8_t key[32] = {0};
    for (int i = 0; i < 32; ++i) key[i] = static_cast<uint8_t>(0xA0 + i % 16);

    const char* msg = "nonce-reuse detection payload";
    const uint8_t* m = reinterpret_cast<const uint8_t*>(msg);
    const size_t mLen = std::strlen(msg);

    // First use of (key, nonceA) encrypts normally.
    uint8_t nonceA[12] = {0,1,2,3,4,5,6,7,8,9,10,11};
    uint8_t tagA[16];
    auto first = crypto.encrypt(m, mLen, key, sizeof(key), nonceA, sizeof(nonceA), tagA, sizeof(tagA));
    REQUIRE_FALSE(first.empty());
    REQUIRE_FALSE(crypto.decrypt(first.data(), first.size(), key, sizeof(key),
                                 nonceA, sizeof(nonceA), tagA, sizeof(tagA)).empty());

    // Reusing the exact same (key, nonceA) must be REJECTED (empty output).
    uint8_t tagReuse[16];
    auto reused = crypto.encrypt(m, mLen, key, sizeof(key), nonceA, sizeof(nonceA), tagReuse, sizeof(tagReuse));
    CHECK(reused.empty());

    // A fresh nonce under the same key is not reuse and succeeds.
    uint8_t nonceB[12] = {20,21,22,23,24,25,26,27,28,29,30,31};
    uint8_t tagB[16];
    auto fresh = crypto.encrypt(m, mLen, key, sizeof(key), nonceB, sizeof(nonceB), tagB, sizeof(tagB));
    REQUIRE_FALSE(fresh.empty());

    // The same nonce (nonceA) under a DIFFERENT key is not reuse and succeeds.
    uint8_t key2[32] = {0};
    for (int i = 0; i < 32; ++i) key2[i] = static_cast<uint8_t>(0xE0 + i % 16);
    uint8_t tagDiff[16];
    auto diffKey = crypto.encrypt(m, mLen, key2, sizeof(key2), nonceA, sizeof(nonceA), tagDiff, sizeof(tagDiff));
    REQUIRE_FALSE(diffKey.empty());
}

TEST_CASE("Crypto: nonce-reuse detection can be disabled (opt-out)") {
    // Save the default so this test never leaks state to the rest of the suite.
    const bool wasEnabled = Caesura::carc::CryptoEngine::nonceReuseDetectionEnabled();
    Caesura::carc::CryptoEngine::setNonceReuseDetection(false);

    Caesura::carc::CryptoEngine crypto;
    uint8_t key[32] = {0};
    for (int i = 0; i < 32; ++i) key[i] = static_cast<uint8_t>(0xB0 + i % 16);
    uint8_t nonce[12] = {40,41,42,43,44,45,46,47,48,49,50,51};
    const char* msg = "opt-out reuse should be permitted";
    const uint8_t* m = reinterpret_cast<const uint8_t*>(msg);
    const size_t mLen = std::strlen(msg);

    uint8_t t1[16], t2[16];
    auto first = crypto.encrypt(m, mLen, key, sizeof(key), nonce, sizeof(nonce), t1, sizeof(t1));
    REQUIRE_FALSE(first.empty());
    // With detection disabled, the same (key, nonce) is allowed again.
    auto second = crypto.encrypt(m, mLen, key, sizeof(key), nonce, sizeof(nonce), t2, sizeof(t2));
    REQUIRE_FALSE(second.empty());

    // Restore the prior state for subsequent tests.
    Caesura::carc::CryptoEngine::setNonceReuseDetection(wasEnabled);
    CHECK(Caesura::carc::CryptoEngine::nonceReuseDetectionEnabled() == wasEnabled);
}

// --- Nonce-reuse detection: CARC normal write path is unaffected -----------
TEST_CASE("CARC: multi-archive write still succeeds with nonce-reuse detection on") {
    // Two consecutive archives in the same process must both finalize cleanly
    // with the (default-on) nonce-reuse registry active -- the per-file nonces
    // are CSPRNG and the per-archive index key is keyed from each archive's own
    // freshly generated public key, so no (key, nonce) pair repeats across
    // archives. This guards against a regression that would break the writer.
    namespace fs = std::filesystem;
    Caesura::TestPaths::ScopedTempDir temp("archive_nonce_multi");
    const bool wasEnabled = Caesura::carc::CryptoEngine::nonceReuseDetectionEnabled();
    Caesura::carc::CryptoEngine::setNonceReuseDetection(true);

    for (int i = 0; i < 2; ++i) {
        const fs::path arc = temp.path() / ("a" + std::to_string(i) + ".carc");
        {
            Caesura::carc::CARCWriter writer;
            REQUIRE(writer.create(arc.string()));
            REQUIRE(writer.addFile("f.txt", reinterpret_cast<const uint8_t*>("hi"), 2));
            REQUIRE(writer.addFile("g.txt", reinterpret_cast<const uint8_t*>("there"), 5));
            REQUIRE(writer.finalize());
        }
        Caesura::carc::CARCReader reader;
        REQUIRE(reader.open(arc.string()));
        CHECK(reader.numFiles() == 2);
        auto got = reader.readFile("f.txt");
        REQUIRE_FALSE(got.empty());
        CHECK(std::memcmp(got.data(), "hi", 2) == 0);
    }

    Caesura::carc::CryptoEngine::setNonceReuseDetection(wasEnabled);
}


// --- DeltaCARC: apply rejects tampered / truncated / missing ---

TEST_CASE("DeltaCARC G11: apply rejects body-tampered and truncated deltas") {
    namespace fs = std::filesystem;
    Caesura::TestPaths::ScopedTempDir temp("archive_g11_deltatamper");
    using TextFiles = std::vector<std::pair<std::string, std::string>>;
    const fs::path oldPath = buildTextCarc(temp.path(), TextFiles{{"a.txt", "one"}}, "old.carc");
    const fs::path newPath = buildTextCarc(temp.path(), TextFiles{{"a.txt", "two"}}, "new.carc");
    const fs::path delta = temp.path() / "delta.bin";
    const fs::path out = temp.path() / "out.carc";

    REQUIRE(carc::DeltaCARC::generate(oldPath.string(), newPath.string(), delta.string()));
    REQUIRE(carc::DeltaCARC::verify(delta.string()));
    REQUIRE(fs::file_size(delta) > 140);

    // Body-tampered copy: flip a byte inside the encrypted body (offset >= 140).
    {
        const fs::path bad = temp.path() / "tampered.bin";
        fs::copy_file(delta, bad, fs::copy_options::overwrite_existing);
        flipByte(bad, 148);
        CHECK_FALSE(carc::DeltaCARC::verify(bad.string()));
        CHECK_FALSE(carc::DeltaCARC::apply(oldPath.string(), bad.string(), out.string()));
    }

    // Truncated copy (header + key material only): decrypt fails / count mismatch.
    {
        const fs::path bad = temp.path() / "truncated.bin";
        {
            std::ifstream in(delta, std::ios::binary);
            std::ofstream outFile(bad, std::ios::binary);
            std::vector<char> buf(140);
            in.read(buf.data(), 140);
            outFile.write(buf.data(), in.gcount());
        }
        CHECK_FALSE(carc::DeltaCARC::verify(bad.string()));
        CHECK_FALSE(carc::DeltaCARC::apply(oldPath.string(), bad.string(), out.string()));
    }

    // Missing source: apply must fail cleanly.
    CHECK_FALSE(carc::DeltaCARC::apply((temp.path() / "missing.carc").string(),
                                       delta.string(), out.string()));

    // A clean apply still succeeds afterwards (delta file itself is untouched).
    REQUIRE(carc::DeltaCARC::apply(oldPath.string(), delta.string(), out.string()));
    carc::CARCReader applied;
    REQUIRE(applied.open(out.string()));
    CHECK(readText(applied, "a.txt") == "two");
}

TEST_CASE("CarcAssetProvider: dynamic priority and custom source name") {
    auto reader = std::make_unique<carc::CARCReader>();
    carc::CarcAssetProvider provider(std::move(reader), 40, "CARC:patch.carc");
    CHECK(provider.priority() == 40);
    CHECK(provider.getSource() == "CARC:patch.carc");
}

TEST_CASE("ProviderChain: Multi-CARC Layered VFS priority resolution") {
    namespace fs = std::filesystem;
    Caesura::TestPaths::ScopedTempDir temp("archive_layered_vfs");

    // Base CARC (prio 10)
    fs::path basePath = temp.path() / "base.carc";
    {
        carc::CARCWriter w;
        REQUIRE(w.create(basePath.string()));
        w.addFile("script/start.ks", reinterpret_cast<const uint8_t*>("base script"), 11);
        w.addFile("bg/title.png", reinterpret_cast<const uint8_t*>("base bg"), 7);
        w.addFile("audio/bgm.ogg", reinterpret_cast<const uint8_t*>("base audio"), 10);
        REQUIRE(w.finalize());
    }

    // DLC CARC (prio 30)
    fs::path dlcPath = temp.path() / "dlc_01.carc";
    {
        carc::CARCWriter w;
        REQUIRE(w.create(dlcPath.string()));
        w.addFile("bg/title.png", reinterpret_cast<const uint8_t*>("dlc bg override"), 15);
        w.addFile("script/dlc_extra.ks", reinterpret_cast<const uint8_t*>("dlc extra"), 9);
        REQUIRE(w.finalize());
    }

    // Patch CARC (prio 40)
    fs::path patchPath = temp.path() / "patch.carc";
    {
        carc::CARCWriter w;
        REQUIRE(w.create(patchPath.string()));
        w.addFile("script/start.ks", reinterpret_cast<const uint8_t*>("patched script"), 14);
        REQUIRE(w.finalize());
    }

    auto baseReader = std::make_unique<carc::CARCReader>();
    REQUIRE(baseReader->open(basePath.string()));
    auto dlcReader = std::make_unique<carc::CARCReader>();
    REQUIRE(dlcReader->open(dlcPath.string()));
    auto patchReader = std::make_unique<carc::CARCReader>();
    REQUIRE(patchReader->open(patchPath.string()));

    ProviderChain chain;
    // Add out of order: base, patch, dlc
    chain.addProvider(std::make_unique<carc::CarcAssetProvider>(std::move(baseReader), 10, "CARC:base"));
    chain.addProvider(std::make_unique<carc::CarcAssetProvider>(std::move(patchReader), 40, "CARC:patch"));
    chain.addProvider(std::make_unique<carc::CarcAssetProvider>(std::move(dlcReader), 30, "CARC:dlc"));

    // 1. Patch (40) overrides Base (10) for script/start.ks
    auto startData = chain.read("script/start.ks");
    REQUIRE_FALSE(startData.empty());
    CHECK(std::string(startData.begin(), startData.end()) == "patched script");

    // 2. DLC (30) overrides Base (10) for bg/title.png
    auto bgData = chain.read("bg/title.png");
    REQUIRE_FALSE(bgData.empty());
    CHECK(std::string(bgData.begin(), bgData.end()) == "dlc bg override");

    // 3. Base (10) serves non-overridden audio/bgm.ogg
    auto bgmData = chain.read("audio/bgm.ogg");
    REQUIRE_FALSE(bgmData.empty());
    CHECK(std::string(bgmData.begin(), bgmData.end()) == "base audio");

    // 4. DLC expansion script/dlc_extra.ks is present
    CHECK(chain.exists("script/dlc_extra.ks"));
    auto extraData = chain.read("script/dlc_extra.ks");
    REQUIRE_FALSE(extraData.empty());
    CHECK(std::string(extraData.begin(), extraData.end()) == "dlc extra");

    // 5. Missing file
    CHECK_FALSE(chain.exists("nonexistent.png"));
    CHECK(chain.read("nonexistent.png").empty());
}

// ===========================================================================
// C1 mount policy (t13): the layered-VFS mount PLAN, not just the chain.
//
// The chain test above proves priority resolution once providers exist. These
// pin the step before it -- which archives the composition root decides to
// mount at all, at which priority, and in which order -- because that is where
// the two real hazards live: a name pattern silently gaining top priority, and
// the same archive being mounted twice.
// ===========================================================================

namespace Caesura {
// Declared in src/entry/Engine_Assets.cpp (composition root).
int carcMountPriority(const std::string& filename, bool inDlcDir);
std::vector<std::pair<std::string, int>> carcMountPlan(const std::string& root);
}  // namespace Caesura

TEST_CASE("Layered VFS C1: mount priority table is exactly the documented one") {
    // patch layer -- highest, wins over everything shipped
    CHECK(Caesura::carcMountPriority("patch.carc", false) == 40);
    CHECK(Caesura::carcMountPriority("patch_001.carc", false) == 40);
    CHECK(Caesura::carcMountPriority("patch_hotfix_v2.carc", false) == 40);
    // dlc -- by name prefix, or by living in dlc/
    CHECK(Caesura::carcMountPriority("dlc_extra.carc", false) == 30);
    CHECK(Caesura::carcMountPriority("anything.carc", true) == 30);
    // localization packs
    CHECK(Caesura::carcMountPriority("lang_en.carc", false) == 20);
    CHECK(Caesura::carcMountPriority("lang_zh.carc", false) == 20);
    // shipped data
    CHECK(Caesura::carcMountPriority("base.carc", false) == 10);
    CHECK(Caesura::carcMountPriority("data.carc", false) == 10);
    CHECK(Caesura::carcMountPriority("game.carc", false) == 10);

    // Anything unrecognized must stay INERT (-1), never join the chain at a
    // guessed priority. A stray archive dropped next to the executable is not
    // silently promoted into the asset path.
    CHECK(Caesura::carcMountPriority("random.carc", false) == -1);
    CHECK(Caesura::carcMountPriority("save_backup.carc", false) == -1);
    CHECK(Caesura::carcMountPriority("Patch.carc", false) == -1);   // case-sensitive by design
    CHECK(Caesura::carcMountPriority("mypatch_1.carc", false) == -1); // prefix, not substring
    CHECK(Caesura::carcMountPriority("", false) == -1);
}

TEST_CASE("Layered VFS C1: mount plan orders by priority and mounts each archive once") {
    namespace fs = std::filesystem;
    Caesura::TestPaths::ScopedTempDir temp("archive_mount_plan");
    const fs::path root = temp.path();

    auto writeArchive = [](const fs::path& p, const char* body) {
        carc::CARCWriter w;
        REQUIRE(w.create(p.string()));
        w.addFile("probe.txt", reinterpret_cast<const uint8_t*>(body),
                  static_cast<uint32_t>(std::strlen(body)));
        REQUIRE(w.finalize());
    };

    writeArchive(root / "base.carc", "base");
    writeArchive(root / "patch_002.carc", "patch2");
    writeArchive(root / "patch_001.carc", "patch1");
    writeArchive(root / "lang_en.carc", "lang");
    writeArchive(root / "unrelated.carc", "inert");     // must NOT be mounted
    fs::create_directories(root / "dlc");
    writeArchive(root / "dlc" / "chapter2.carc", "dlc-by-directory");

    const auto plan = Caesura::carcMountPlan(root.string());

    // The inert archive is absent; everything else is mounted exactly once.
    // (Regression guard: keying dedup on the iterator's path spelling while
    // re-checking a fallback list by bare name mounted base.carc TWICE --
    // two providers, two open streams, one file.)
    CHECK(plan.size() == 5);
    int baseCount = 0;
    for (const auto& entry : plan) {
        CHECK(entry.first != "unrelated.carc");
        if (entry.first == "base.carc") baseCount++;
    }
    CHECK(baseCount == 1);

    // Descending priority, ties broken by name for determinism.
    CHECK(plan[0] == std::make_pair(std::string("patch_001.carc"), 40));
    CHECK(plan[1] == std::make_pair(std::string("patch_002.carc"), 40));
    CHECK(plan[2] == std::make_pair(std::string("chapter2.carc"), 30));
    CHECK(plan[3] == std::make_pair(std::string("lang_en.carc"), 20));
    CHECK(plan[4] == std::make_pair(std::string("base.carc"), 10));

    // Non-decreasing check stated independently of the exact names above, so
    // adding a layer later cannot quietly break the ordering invariant.
    for (size_t i = 1; i < plan.size(); ++i) {
        CHECK(plan[i - 1].second >= plan[i].second);
    }
}

TEST_CASE("Layered VFS C1: mount plan is empty for a directory with no archives") {
    Caesura::TestPaths::ScopedTempDir temp("archive_mount_plan_empty");
    CHECK(Caesura::carcMountPlan(temp.path().string()).empty());
    // A path that does not exist must be handled as "nothing to mount", not a throw.
    CHECK(Caesura::carcMountPlan((temp.path() / "does_not_exist").string()).empty());
}

// ===========================================================================
// C2 delta tooling (t13): the full generate -> verify -> apply round trip
// asserted on CONTENT, not just on return codes.
//
// The existing DeltaCARC G11 case covers rejection of tampered/truncated
// deltas. What was missing is the positive path a release engineer actually
// runs: change some files, add one, remove one, and confirm the rebuilt
// archive is byte-identical to the intended target.
// ===========================================================================

TEST_CASE("DeltaCARC C2: generate/verify/apply round trip reproduces the target exactly") {
    namespace fs = std::filesystem;
    Caesura::TestPaths::ScopedTempDir temp("archive_delta_roundtrip");

    const fs::path basePath  = temp.path() / "base.carc";
    const fs::path newPath   = temp.path() / "new.carc";
    const fs::path deltaPath = temp.path() / "patch.carc";
    const fs::path outPath   = temp.path() / "rebuilt.carc";

    // base: a.ks (will change), b.png (untouched), gone.txt (will be removed)
    {
        carc::CARCWriter w;
        REQUIRE(w.create(basePath.string()));
        w.addFile("a.ks", reinterpret_cast<const uint8_t*>("scene one v1"), 12);
        w.addFile("b.png", reinterpret_cast<const uint8_t*>("shared asset"), 12);
        w.addFile("gone.txt", reinterpret_cast<const uint8_t*>("to be removed"), 13);
        REQUIRE(w.finalize());
    }
    // target: a.ks changed, b.png identical, gone.txt dropped, c.ogg added
    {
        carc::CARCWriter w;
        REQUIRE(w.create(newPath.string()));
        w.addFile("a.ks", reinterpret_cast<const uint8_t*>("scene one v2 CHANGED"), 20);
        w.addFile("b.png", reinterpret_cast<const uint8_t*>("shared asset"), 12);
        w.addFile("c.ogg", reinterpret_cast<const uint8_t*>("brand new file"), 14);
        REQUIRE(w.finalize());
    }

    REQUIRE(carc::DeltaCARC::generate(basePath.string(), newPath.string(), deltaPath.string()));
    REQUIRE(fs::exists(deltaPath));
    REQUIRE(carc::DeltaCARC::verify(deltaPath.string()));
    REQUIRE(carc::DeltaCARC::apply(basePath.string(), deltaPath.string(), outPath.string()));

    carc::CARCReader rebuilt;
    REQUIRE(rebuilt.open(outPath.string()));
    carc::CARCReader target;
    REQUIRE(target.open(newPath.string()));

    // Same file set, same count -- the removed entry really is gone and the
    // added one really arrived.
    CHECK(rebuilt.numFiles() == target.numFiles());
    CHECK(rebuilt.hasFile("a.ks"));
    CHECK(rebuilt.hasFile("b.png"));
    CHECK(rebuilt.hasFile("c.ogg"));
    CHECK_FALSE(rebuilt.hasFile("gone.txt"));

    // Content equality per entry (updated, untouched, and newly added).
    CHECK(readText(rebuilt, "a.ks") == "scene one v2 CHANGED");
    CHECK(readText(rebuilt, "b.png") == "shared asset");
    CHECK(readText(rebuilt, "c.ogg") == "brand new file");
    CHECK(readText(rebuilt, "a.ks") == readText(target, "a.ks"));
    CHECK(readText(rebuilt, "b.png") == readText(target, "b.png"));
    CHECK(readText(rebuilt, "c.ogg") == readText(target, "c.ogg"));

    // Applying to the WRONG base must fail: the delta header pins the source
    // SHA-256, so a patch cannot be smeared onto an unrelated archive.
    const fs::path wrongBase = temp.path() / "wrong.carc";
    {
        carc::CARCWriter w;
        REQUIRE(w.create(wrongBase.string()));
        w.addFile("a.ks", reinterpret_cast<const uint8_t*>("unrelated"), 9);
        REQUIRE(w.finalize());
    }
    CHECK_FALSE(carc::DeltaCARC::apply(wrongBase.string(), deltaPath.string(),
                                       (temp.path() / "bad.carc").string()));
}

