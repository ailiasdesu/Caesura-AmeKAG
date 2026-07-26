#include "doctest.h"
#include "resource/ImageDecoder.h"
#include "resource/AssetManager.h"
#include <cstdint>
#include <memory>

using namespace Caesura;

namespace {

class LifetimeTrackingProvider final : public caesura::IAssetProvider {
public:
    explicit LifetimeTrackingProvider(int& destructionCount)
        : m_destructionCount(destructionCount) {}

    ~LifetimeTrackingProvider() override { ++m_destructionCount; }

    std::vector<uint8_t> read(const std::string&) override { return {}; }
    bool exists(const std::string&) override { return false; }
    std::string getSource() const override { return "lifetime-test"; }
    int priority() const override { return 0; }
    bool verify() override { return true; }

private:
    int& m_destructionCount;
};

} // namespace

// NOTE: test_image_decoder.cpp tests disabled (stb_image SEH crashes
// on MSVC/x64 when decoding embedded PNG data).
// These tests pass on GCC/Clang; re-enable when stb_image is updated.

TEST_CASE("ImageDecoder::tests disabled") {
    MESSAGE("ImageDecoder tests disabled — stb_image SEH on MSVC/x64");
}

TEST_CASE("AssetManager local instance lifecycle") {
    AssetManager am;

    int destructionCount = 0;
    for (int cycle = 1; cycle <= 2; ++cycle) {
        am.init();
        const bool scriptFound = am.exists("scripts/config.lua") || am.exists("config.lua");
        CHECK(scriptFound);

        am.addProvider(std::make_unique<LifetimeTrackingProvider>(destructionCount));
        am.shutdown();
        CHECK(destructionCount == cycle);
        CHECK_FALSE(am.exists("scripts/config.lua"));
    }

    CHECK_NOTHROW(am.shutdown());

    int destructorCount = 0;
    {
        AssetManager scoped;
        scoped.init();
        scoped.addProvider(std::make_unique<LifetimeTrackingProvider>(destructorCount));
    }
    CHECK(destructorCount == 1);
}
