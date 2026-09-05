#pragma once

#include "doctest.h"
#include "TestPaths.h"
#include "archive/CARCReader.h"
#include "archive/CARCWriter.h"
#include "archive/CryptoEngine.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace Caesura::Test {

// A writer-produced corpus, following test_carc.cpp's private-key/re-sign
// fixture pattern. Re-encryption rotates the publisher identity, so the
// production process-wide nonce-reuse detector can remain enabled.
struct U9CarcFixture {
    using Bytes = std::vector<uint8_t>;
    using Files = std::vector<std::pair<std::string, Bytes>>;
    TestPaths::ScopedTempDir temp{"u9_carc"};
    const std::filesystem::path path = temp.path() / "sample.carc";
    const std::filesystem::path publicKeyPath = temp.path() / "publisher.pub";
    const std::filesystem::path privateKeyPath = temp.path() / "publisher.key";
    Files files;
    carc::ArchivePublicKey publicKey{};
    carc::CARCHeader header{};
    Bytes indexPlain;
    Bytes bytes;
    Bytes headerAndContent;

    static Bytes noise(size_t size) {
        Bytes result(size);
        uint32_t state = 0x12345678;
        for (auto& byte : result) {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            byte = static_cast<uint8_t>(state);
        }
        return result;
    }

    static Files sampleFiles() {
        return {{"text.txt", Bytes(8192, 'A')},
                {"noise.bin", noise(65537)}};
    }

    explicit U9CarcFixture(Files sample = sampleFiles()) : files(std::move(sample)) {
        {
            carc::CARCWriter writer;
            REQUIRE(writer.create(path.string(), privateKeyPath.string(), publicKeyPath.string()));
            for (const auto& [name, data] : files)
                REQUIRE(writer.addFile(name, data.data(), data.size()));
            REQUIRE(writer.finalize());
        }
        REQUIRE(carc::CryptoEngine::readPublicKey(publicKeyPath.string(), publicKey.data()));
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        REQUIRE(input.is_open());
        const auto length = input.tellg();
        REQUIRE(length > static_cast<std::streamoff>(sizeof(header) + carc::SIGNATURE_SIZE + carc::PUBLICKEY_SIZE));
        REQUIRE(length < static_cast<std::streamoff>(1024 * 1024));
        bytes.resize(static_cast<size_t>(length));
        input.seekg(0);
        REQUIRE(input.read(reinterpret_cast<char*>(bytes.data()), length));
        input.close();
        std::memcpy(&header, bytes.data(), sizeof(header));
        REQUIRE(header.indexOffset < bytes.size());
        REQUIRE(header.indexSize >= carc::AES_TAG_SIZE);
        headerAndContent.assign(bytes.begin(), bytes.begin() + static_cast<size_t>(header.indexOffset));
        uint8_t hash[carc::PATH_HASH_SIZE]{};
        uint8_t nonce[carc::AES_NONCE_SIZE]{};
        indexCrypto(publicKey, header.version, hash, nonce);
        const size_t encryptedSize = static_cast<size_t>(header.indexSize - carc::AES_TAG_SIZE);
        indexPlain = carc::CryptoEngine::decrypt(bytes.data() + header.indexOffset,
            encryptedSize, hash, nonce, bytes.data() + header.indexOffset + encryptedSize);
        REQUIRE(indexPlain.size() == sizeof(uint32_t) + files.size() * sizeof(carc::FileEntry));
        carc::CARCReader baseline;
        REQUIRE(baseline.open(path.string(), publicKey));
        for (const auto& [name, data] : files) REQUIRE(baseline.readFile(name) == data);
    }

    static void indexCrypto(const carc::ArchivePublicKey& key, uint32_t version,
                            uint8_t* hash, uint8_t* nonce) {
        carc::CryptoEngine::sha256(key.data(), key.size(), hash);
        std::memcpy(nonce, &version, sizeof(version));
        std::memcpy(nonce + sizeof(version), hash, carc::AES_NONCE_SIZE - sizeof(version));
    }

    carc::FileEntry entry(size_t index) const {
        const size_t offset = sizeof(uint32_t) + index * sizeof(carc::FileEntry);
        REQUIRE(offset + sizeof(carc::FileEntry) <= indexPlain.size());
        carc::FileEntry result{};
        std::memcpy(&result, indexPlain.data() + offset, sizeof(result));
        return result;
    }

    void setEntry(size_t index, const carc::FileEntry& value) {
        const size_t offset = sizeof(uint32_t) + index * sizeof(carc::FileEntry);
        REQUIRE(offset + sizeof(value) <= indexPlain.size());
        std::memcpy(indexPlain.data() + offset, &value, sizeof(value));
    }

    void writeBytes() const {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        REQUIRE(output.is_open());
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
        output.close();
        REQUIRE(output.good());
    }

    void seal(bool corruptIndexTag = false) {
        uint8_t privateKey[64]{};
        carc::CryptoEngine::generateKeyPair(publicKey.data(), privateKey);
        uint8_t hash[carc::PATH_HASH_SIZE]{};
        uint8_t nonce[carc::AES_NONCE_SIZE]{};
        uint8_t tag[carc::AES_TAG_SIZE]{};
        indexCrypto(publicKey, header.version, hash, nonce);
        auto encrypted = carc::CryptoEngine::encrypt(indexPlain.data(), indexPlain.size(), hash, nonce, tag);
        REQUIRE(encrypted.size() == indexPlain.size());
        if (corruptIndexTag) tag[0] ^= 1;
        bytes = headerAndContent;
        std::memcpy(bytes.data(), &header, sizeof(header));
        bytes.insert(bytes.end(), encrypted.begin(), encrypted.end());
        bytes.insert(bytes.end(), tag, tag + sizeof(tag));
        uint8_t signature[carc::SIGNATURE_SIZE]{};
        REQUIRE(carc::CryptoEngine::sign(bytes.data(), bytes.size(), privateKey, signature));
        REQUIRE(carc::CryptoEngine::verify(bytes.data(), bytes.size(), publicKey.data(), signature));
        bytes.insert(bytes.end(), signature, signature + sizeof(signature));
        bytes.insert(bytes.end(), publicKey.begin(), publicKey.end());
        REQUIRE(bytes.size() < 1024 * 1024); // all declared oversizes stay metadata-only
        writeBytes();
    }
};

} // namespace Caesura::Test
