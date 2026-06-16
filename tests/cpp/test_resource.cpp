#include "doctest.h"
#include "resource/ResourceHandle.h"
#include "resource/DirAssetProvider.h"
#include "resource/ProviderChain.h"
#include <cstdio>
#include <fstream>
#include <filesystem>

using namespace Caesura;    // ResourceHandle, GenerationTracker, HandleType::TEXTURE
using namespace caesura;    // DirAssetProvider, ProviderChain

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
    chain.addProvider(std::make_unique<DirAssetProvider>("test_high"));  // priority 5
    chain.addProvider(std::make_unique<DirAssetProvider>("test_low"));   // priority 5, same priority
    // Both have same priority — file should be found via second provider
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
}
