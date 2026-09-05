#pragma once

#include "doctest.h"
#include "TestPaths.h"
#include "archive/CARCReader.h"
#include "archive/CARCWriter.h"
#include "archive/CryptoEngine.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace Caesura::Test {

// Separate generated publisher identities, with mount copies outside their
// key directory. CARCWriter generates a new identity on every create().
struct PublisherArchives {
    TestPaths::ScopedTempDir temp{"u8_publishers"};
    const std::filesystem::path root = temp.path() / "mounts";
    const std::filesystem::path trusted = temp.path() / "publisher_a.carc";
    const std::filesystem::path attacker = temp.path() / "publisher_b.carc";
    const std::filesystem::path trustedKeyPath = temp.path() / "publisher_a.pub";
    const std::filesystem::path attackerKeyPath = temp.path() / "publisher_b.pub";
    carc::ArchivePublicKey trustedKey{};
    carc::ArchivePublicKey attackerKey{};

    PublisherArchives() {
        std::filesystem::create_directories(root / "dlc");
        writeArchive(trusted, trustedKeyPath, "publisher A");
        writeArchive(attacker, attackerKeyPath, "attacker B");
        REQUIRE(carc::CryptoEngine::readPublicKey(trustedKeyPath.string(), trustedKey.data()));
        REQUIRE(carc::CryptoEngine::readPublicKey(attackerKeyPath.string(), attackerKey.data()));
        REQUIRE(trustedKey != attackerKey);
    }

    static void writeArchive(const std::filesystem::path& path,
                             const std::filesystem::path& keyPath,
                             const std::string& payload) {
        carc::CARCWriter writer;
        REQUIRE(writer.create(path.string(), "", keyPath.string()));
        REQUIRE(writer.addFile("u8_publisher_payload.txt",
            reinterpret_cast<const uint8_t*>(payload.data()), payload.size()));
        REQUIRE(writer.finalize());
    }

    std::filesystem::path mount(const std::string& name, bool publisherA) const {
        const auto destination = root / name;
        std::filesystem::create_directories(destination.parent_path());
        std::filesystem::copy_file(publisherA ? trusted : attacker, destination,
            std::filesystem::copy_options::overwrite_existing);
        return destination;
    }

    static void replaceEmbeddedKey(const std::filesystem::path& path,
                                   const carc::ArchivePublicKey& key) {
        std::fstream stream(path, std::ios::binary | std::ios::in | std::ios::out);
        REQUIRE(stream.is_open());
        stream.seekp(-static_cast<std::streamoff>(key.size()), std::ios::end);
        stream.write(reinterpret_cast<const char*>(key.data()), key.size());
        REQUIRE(stream.good());
    }
};

// Engine's production mount root is the process working directory. Keep this
// scope outside Engine so jobs and open archive streams stop before restoring it.
class ScopedWorkingDirectory {
public:
    explicit ScopedWorkingDirectory(const std::filesystem::path& path)
        : m_previous(std::filesystem::current_path()) {
        std::filesystem::current_path(path);
    }
    ~ScopedWorkingDirectory() {
        std::error_code ignored;
        std::filesystem::current_path(m_previous, ignored);
    }
private:
    const std::filesystem::path m_previous;
};

} // namespace Caesura::Test
