// test_delta_carc.cpp - DeltaCARC v2 differential archive tests
#include "doctest.h"
#include "archive/DeltaCARC.h"
#include "archive/CARCReader.h"
#include "archive/CARCWriter.h"
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <utility>
#include <vector>
#include <string>

using namespace Caesura;

namespace {

// Unique temp dir per test run (portable; no Windows-only APIs).
static std::string makeTempDir() {
    static std::atomic<uint64_t> counter{0};
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto dir = std::filesystem::temp_directory_path() /
        ("caesura_delta_test_" + std::to_string(stamp) + "_" + std::to_string(counter++));
    std::filesystem::create_directories(dir);
    return dir.string();
}

// Build a CARC archive from {relativePath, content} pairs.
static std::string buildCarc(const std::string& dir,
                             const std::string& name,
                             const std::vector<std::pair<std::string, std::string>>& files) {
    std::string path = (std::filesystem::path(dir) / name).string();
    carc::CARCWriter w;
    if (!w.create(path)) return "";
    for (const auto& [rel, content] : files) {
        w.addFile(rel, reinterpret_cast<const uint8_t*>(content.data()), content.size());
    }
    if (!w.finalize()) return "";
    return path;
}

static std::string readText(carc::CARCReader& r, const std::string& path) {
    auto data = r.readFile(path);
    return std::string(data.begin(), data.end());
}

} // namespace

TEST_CASE("DeltaCARC::round-trip applies adds and replaces") {
    auto dir = makeTempDir();
    auto oldPath = buildCarc(dir, "old.carc", {
        {"a.txt", "hello"},
        {"b.txt", "world"},
    });
    auto newPath = buildCarc(dir, "new.carc", {
        {"a.txt", "hello v2"},   // replaced
        {"b.txt", "world"},      // unchanged
        {"c.txt", "brand new"},  // added
    });
    auto deltaPath = (std::filesystem::path(dir) / "delta.bin").string();
    auto outPath = (std::filesystem::path(dir) / "out.carc").string();

    REQUIRE(oldPath.size() > 0);
    REQUIRE(newPath.size() > 0);

    CHECK(carc::DeltaCARC::generate(oldPath, newPath, deltaPath));
    CHECK(carc::DeltaCARC::verify(deltaPath));
    CHECK(carc::DeltaCARC::apply(oldPath, deltaPath, outPath));

    carc::CARCReader out;
    REQUIRE(out.open(outPath));
    CHECK(out.numFiles() == 3);
    CHECK(readText(out, "a.txt") == "hello v2");
    CHECK(readText(out, "b.txt") == "world");
    CHECK(readText(out, "c.txt") == "brand new");

    out.close();
    std::filesystem::remove_all(dir);
}

TEST_CASE("DeltaCARC::apply removes deleted files") {
    auto dir = makeTempDir();
    auto oldPath = buildCarc(dir, "old.carc", {
        {"a.txt", "keep a"},
        {"b.txt", "drop b"},
        {"c.txt", "keep c"},
    });
    auto newPath = buildCarc(dir, "new.carc", {
        {"a.txt", "keep a"},
        {"c.txt", "keep c"},
    });
    auto deltaPath = (std::filesystem::path(dir) / "delta.bin").string();
    auto outPath = (std::filesystem::path(dir) / "out.carc").string();

    REQUIRE(oldPath.size() > 0);
    REQUIRE(newPath.size() > 0);

    CHECK(carc::DeltaCARC::generate(oldPath, newPath, deltaPath));
    CHECK(carc::DeltaCARC::apply(oldPath, deltaPath, outPath));

    carc::CARCReader out;
    REQUIRE(out.open(outPath));
    CHECK(out.numFiles() == 2);
    CHECK(out.hasFile("a.txt"));
    CHECK_FALSE(out.hasFile("b.txt"));
    CHECK(out.hasFile("c.txt"));
    CHECK(readText(out, "a.txt") == "keep a");
    CHECK(readText(out, "c.txt") == "keep c");

    out.close();
    std::filesystem::remove_all(dir);
}

TEST_CASE("DeltaCARC::round-trip preserves binary content") {
    auto dir = makeTempDir();
    std::string binaryContent;
    for (int i = 0; i < 256; i++) binaryContent.push_back(static_cast<char>(i));
    auto oldPath = buildCarc(dir, "old.carc", {{"data.bin", "old"}});
    auto newPath = buildCarc(dir, "new.carc", {{"data.bin", binaryContent}});
    auto deltaPath = (std::filesystem::path(dir) / "delta.bin").string();
    auto outPath = (std::filesystem::path(dir) / "out.carc").string();

    REQUIRE(oldPath.size() > 0);
    REQUIRE(newPath.size() > 0);

    CHECK(carc::DeltaCARC::generate(oldPath, newPath, deltaPath));
    CHECK(carc::DeltaCARC::apply(oldPath, deltaPath, outPath));

    carc::CARCReader out;
    REQUIRE(out.open(outPath));
    auto data = out.readFile("data.bin");
    CHECK(data.size() == binaryContent.size());
    CHECK(std::memcmp(data.data(), binaryContent.data(), binaryContent.size()) == 0);

    out.close();
    std::filesystem::remove_all(dir);
}

TEST_CASE("DeltaCARC::apply rejects mismatched source") {
    auto dir = makeTempDir();
    auto sourceA = buildCarc(dir, "a.carc", {{"x.txt", "xxx"}});
    auto sourceB = buildCarc(dir, "b.carc", {{"y.txt", "yyy"}});
    auto target = buildCarc(dir, "t.carc", {{"x.txt", "xxx v2"}});
    auto deltaPath = (std::filesystem::path(dir) / "delta.bin").string();
    auto outPath = (std::filesystem::path(dir) / "out.carc").string();

    REQUIRE(sourceA.size() > 0);
    REQUIRE(sourceB.size() > 0);
    REQUIRE(target.size() > 0);

    REQUIRE(carc::DeltaCARC::generate(sourceA, target, deltaPath));
    // Applying a delta built for sourceA to unrelated sourceB must fail.
    CHECK_FALSE(carc::DeltaCARC::apply(sourceB, deltaPath, outPath));

    std::filesystem::remove_all(dir);
}

TEST_CASE("DeltaCARC::verify rejects corrupt deltas") {
    auto dir = makeTempDir();
    auto oldPath = buildCarc(dir, "old.carc", {{"a.txt", "one"}});
    auto newPath = buildCarc(dir, "new.carc", {{"a.txt", "two"}});
    auto deltaPath = (std::filesystem::path(dir) / "delta.bin").string();

    REQUIRE(oldPath.size() > 0);
    REQUIRE(newPath.size() > 0);
    REQUIRE(carc::DeltaCARC::generate(oldPath, newPath, deltaPath));

    // Intact delta verifies.
    CHECK(carc::DeltaCARC::verify(deltaPath));

    // Corrupt magic byte.
    {
        std::fstream f(deltaPath, std::ios::binary | std::ios::in | std::ios::out);
        char bad = static_cast<char>(~carc::DELTA_MAGIC & 0xFF);
        f.seekp(0);
        f.write(&bad, 1);
    }
    CHECK_FALSE(carc::DeltaCARC::verify(deltaPath));

    // Truncated file (header + key material only).
    {
        auto truncated = (std::filesystem::path(dir) / "truncated.bin").string();
        std::ifstream in(deltaPath, std::ios::binary);
        std::ofstream out(truncated, std::ios::binary);
        std::vector<char> buf(64);
        in.read(buf.data(), 64);
        out.write(buf.data(), in.gcount());
    }
    {
        auto truncated = (std::filesystem::path(dir) / "truncated.bin").string();
        CHECK_FALSE(carc::DeltaCARC::verify(truncated));
    }

    // Missing file.
    CHECK_FALSE(carc::DeltaCARC::verify((std::filesystem::path(dir) / "nope.bin").string()));

    std::filesystem::remove_all(dir);
}
