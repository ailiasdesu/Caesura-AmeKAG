#include "doctest.h"
#include "resource/ResourceHandle.h"
#include "resource/DirAssetProvider.h"
#include "resource/ProviderChain.h"
#include "resource/XP3Archive.h"
#include "TestPaths.h"
#include <cstdio>
#include <fstream>
#include <filesystem>

using namespace Caesura;    // ResourceHandle, GenerationTracker, HandleType::TEXTURE
using namespace Caesura;    // DirAssetProvider, ProviderChain

TEST_CASE("ResourceHandle::default") {
    ResourceHandle h;
    CHECK(h.id == 0);
    CHECK_FALSE(h);
}

TEST_CASE("GenerationTracker::invalidate") {
    GenerationTracker gt;
    auto h = gt.makeHandle(HandleType::TEXTURE, 42);
    CHECK(h.id == 42);
    CHECK(gt.isCurrent(h));
    gt.invalidate(HandleType::TEXTURE);
    CHECK_FALSE(gt.isCurrent(h));
}

TEST_CASE("GenerationTracker::independent types") {
    GenerationTracker gt;
    auto h1 = gt.makeHandle(HandleType::TEXTURE, 1);
    auto h2 = gt.makeHandle(HandleType::AUDIO, 1);
    gt.invalidate(HandleType::TEXTURE);
    CHECK_FALSE(gt.isCurrent(h1));
    CHECK(gt.isCurrent(h2));
}

TEST_CASE("DirAssetProvider::exists") {
    namespace fs = std::filesystem;
    fs::create_directories("test_assets");
    std::ofstream("test_assets/hello.txt") << "world";
    DirAssetProvider provider("test_assets");
    CHECK(provider.exists("hello.txt"));
    CHECK_FALSE(provider.exists("ghost.txt"));
    fs::remove_all("test_assets");
}

TEST_CASE("ProviderChain::add and check") {
    namespace fs = std::filesystem;
    fs::create_directories("test_pc");
    std::ofstream f("test_pc/data.bin", std::ios::binary);
    f.write("binary", 6);
    f.close();
    ProviderChain chain;
    chain.addProvider(std::make_unique<DirAssetProvider>("test_pc"));
    CHECK(chain.exists("data.bin"));
    CHECK_FALSE(chain.exists("nope.bin"));
    fs::remove_all("test_pc");
}

// =============================================================================
// Expanded: DirAssetProvider read + ProviderChain priority fallback
// =============================================================================

TEST_CASE("DirAssetProvider::read returns file content") {
    namespace fs = std::filesystem;
    fs::create_directories("test_read");
    { std::ofstream out("test_read/data.txt"); out << "hello world"; }
    DirAssetProvider provider("test_read");
    auto data = provider.read("data.txt");
    REQUIRE_FALSE(data.empty());
    std::string content(data.begin(), data.end());
    CHECK(content == "hello world");
    fs::remove_all("test_read");
}

TEST_CASE("DirAssetProvider::getSource and priority") {
    DirAssetProvider provider("/some/root");
    CHECK(provider.getSource().find("Dir:") != std::string::npos);
    CHECK(provider.getSource().find("/some/root") != std::string::npos);
    CHECK(provider.priority() == 5);
    CHECK(provider.verify());
}

TEST_CASE("ProviderChain::read falls back to lower priority") {
    namespace fs = std::filesystem;
    fs::create_directories("test_high");
    fs::create_directories("test_low");
    // Only write file in low-priority dir
    { std::ofstream out("test_low/fallback.txt"); out << "fallback"; }
    ProviderChain chain;
    // Both providers have default priority 5. File only in test_low —
    // tests same-priority chaining (not priority ordering).
    chain.addProvider(std::make_unique<DirAssetProvider>("test_high"));
    chain.addProvider(std::make_unique<DirAssetProvider>("test_low"));
    CHECK(chain.exists("fallback.txt"));
    auto data = chain.read("fallback.txt");
    REQUIRE_FALSE(data.empty());
    std::string content(data.begin(), data.end());
    CHECK(content == "fallback");
    fs::remove_all("test_high");
    fs::remove_all("test_low");
}

TEST_CASE("ProviderChain::providers accessor") {
    ProviderChain chain;
    CHECK(chain.providers().empty());
    chain.addProvider(std::make_unique<DirAssetProvider>("some_dir"));
    CHECK(chain.providers().size() == 1);
    chain.clear();
    CHECK(chain.providers().empty());
    CHECK_NOTHROW(chain.clear());
}

TEST_CASE("DirAssetProvider::read nonexistent returns empty") {
    namespace fs = std::filesystem;
    fs::create_directories("test_empty");
    DirAssetProvider provider("test_empty");
    auto data = provider.read("nonexistent.txt");
    CHECK(data.empty());
    fs::remove_all("test_empty");
}

TEST_CASE("DirAssetProvider confines reads to its root") {
    namespace fs = std::filesystem;
    TestPaths::ScopedTempDir temp("asset_provider_confinement");
    const fs::path root = temp.path() / "assets";
    const fs::path outside = temp.path() / "secret.txt";
    fs::create_directories(root);
    { std::ofstream secret(outside); secret << "not an asset"; }

    DirAssetProvider rooted(root.string());
    CHECK_FALSE(rooted.exists("../secret.txt"));
    CHECK(rooted.read("../secret.txt").empty());

    DirAssetProvider workingDirectoryRoot("");
    CHECK_FALSE(workingDirectoryRoot.exists(fs::absolute(outside).string()));
    CHECK(workingDirectoryRoot.read(fs::absolute(outside).string()).empty());
}

TEST_CASE("XP3Archive rejects raw segment size mismatch") {
    namespace fs = std::filesystem;
    TestPaths::ScopedTempDir temp("xp3_raw_size_mismatch");
    const fs::path archivePath = temp.path() / "malformed.xp3";
    const fs::path outputDir = temp.path() / "output";

    std::vector<uint8_t> bytes;
    auto appendU32 = [&bytes](uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8)
            bytes.push_back(static_cast<uint8_t>(value >> shift));
    };
    auto appendU64 = [&bytes](uint64_t value) {
        for (int shift = 0; shift < 64; shift += 8)
            bytes.push_back(static_cast<uint8_t>(value >> shift));
    };

    const char magic[] = "XP3\r\n";
    bytes.insert(bytes.end(), magic, magic + 5);
    appendU64(14); // one data byte starts at offset 13; index starts at 14
    bytes.push_back('X');

    appendU32(0); // file flags
    appendU64(4); // declared original file size
    appendU64(1); // declared archived file size
    bytes.push_back('a'); bytes.push_back(0); // UTF-16LE filename
    bytes.push_back(0);   bytes.push_back(0); // terminator
    appendU32(1); // one segment
    appendU32(XP3Archive::XP3_ENC_RAW);
    appendU64(13); // data offset
    appendU64(4);  // original segment size
    appendU64(1);  // archived segment size

    std::ofstream out(archivePath, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();

    CHECK_FALSE(XP3Archive::unpack(archivePath.string(), outputDir.string()));
    CHECK_FALSE(fs::exists(outputDir / "a"));
}

TEST_CASE("XP3Archive pack list and unpack round-trip") {
    namespace fs = std::filesystem;
    TestPaths::ScopedTempDir temp("xp3_roundtrip");
    const fs::path inputDir = temp.path() / "input";
    const fs::path outputDir = temp.path() / "output";
    const fs::path archivePath = temp.path() / "roundtrip.xp3";
    fs::create_directories(inputDir / "nested");
    {
        std::ofstream source(inputDir / "nested" / "story.txt", std::ios::binary);
        source << "Caesura XP3 round-trip payload";
    }

    REQUIRE(XP3Archive::pack(inputDir.string(), archivePath.string()));
    const auto entries = XP3Archive::list(archivePath.string());
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].orgSize == 30);

    REQUIRE(XP3Archive::unpack(archivePath.string(), outputDir.string()));
    std::ifstream extracted(outputDir / "nested" / "story.txt", std::ios::binary);
    REQUIRE(extracted.is_open());
    const std::string contents((std::istreambuf_iterator<char>(extracted)),
                               std::istreambuf_iterator<char>());
    CHECK(contents == "Caesura XP3 round-trip payload");
}
