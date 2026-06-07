#include "doctest.h"
#include "Resource/ResourceHandle.h"
#include "Resource/DirAssetProvider.h"
#include "Resource/ProviderChain.h"
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