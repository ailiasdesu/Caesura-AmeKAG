// U9: bounded, signed CARC parser/decryption and shared-reader regressions.
#include "doctest.h"
#include "U9CarcFixture.h"
#include <atomic>
#include <limits>
#include <thread>

using Caesura::Test::U9CarcFixture;
using namespace Caesura::carc;

namespace {
void checkClosed(const CARCReader& reader) {
    CHECK_FALSE(reader.isOpen());
    CHECK_FALSE(reader.hasPublicKey());
    CHECK(reader.numFiles() == 0);
    CHECK(reader.index().empty());
    CHECK(reader.fileList().empty());
    CHECK_FALSE(reader.hasFile("text.txt"));
}

void checkRejected(const U9CarcFixture& fixture) {
    CARCReader reader;
    bool opened = false;
    CHECK_NOTHROW(opened = reader.open(fixture.path.string(), fixture.publicKey));
    CHECK_FALSE(opened);
    checkClosed(reader);
    CHECK(reader.readFile("text.txt").empty());
}
} // namespace

TEST_CASE("U9: signed structural overflow and region declarations reject before allocation") {
    U9CarcFixture fixture;
    const auto max = (std::numeric_limits<uint64_t>::max)();
    SUBCASE("content end overflows uint64") {
        fixture.header.contentSize = max;
    }
    SUBCASE("index end overflows after a representable content end") {
        fixture.header.indexOffset = max - 1;
        fixture.header.contentSize = fixture.header.indexOffset - fixture.header.contentOffset;
    }
    SUBCASE("content starts inside header") {
        fixture.header.contentOffset = sizeof(CARCHeader) - 1;
    }
    SUBCASE("content overlaps index by one byte") {
        ++fixture.header.contentSize;
    }
    SUBCASE("content leaves a gap before index") {
        --fixture.header.contentSize;
    }
    SUBCASE("declared index enters trailer") {
        ++fixture.header.indexSize;
    }
    SUBCASE("index declaration is above 64 MiB") {
        fixture.header.indexSize = 64ull * 1024 * 1024 + 1;
    }
    SUBCASE("signed region declaration is above 1 GiB") {
        fixture.header.indexOffset = 1024ull * 1024 * 1024;
        fixture.header.contentSize = fixture.header.indexOffset - fixture.header.contentOffset;
    }
    SUBCASE("file count exceeds the bounded index capacity") {
        fixture.header.numFiles = (std::numeric_limits<uint32_t>::max)();
    }
    // Signature is valid over the actual physical body; malformed length
    // declarations intentionally fail structural validation before verification.
    fixture.seal();
    checkRejected(fixture);
}

TEST_CASE("U9: authenticated index count and entry extent inconsistencies reject") {
    U9CarcFixture fixture;
    auto entry = fixture.entry(0);
    SUBCASE("decrypted count differs from header") {
        const uint32_t wrongCount = fixture.header.numFiles + 1;
        std::memcpy(fixture.indexPlain.data(), &wrongCount, sizeof(wrongCount));
    }
    SUBCASE("entry offset is beyond content") {
        entry.offset = fixture.header.contentSize + 1;
        fixture.setEntry(0, entry);
    }
    SUBCASE("entry offset at end cannot carry nonempty content") {
        entry.offset = fixture.header.contentSize;
        fixture.setEntry(0, entry);
    }
    SUBCASE("entry region crosses content end by one byte") {
        entry.compressedSize = fixture.header.contentSize - entry.offset + 1;
        fixture.setEntry(0, entry);
    }
    SUBCASE("entry extent would wrap if added unchecked") {
        entry.offset = (std::numeric_limits<uint64_t>::max)();
        entry.compressedSize = 2;
        fixture.setEntry(0, entry);
    }
    SUBCASE("entry original size above 512 MiB is metadata only") {
        entry.originalSize = 512ull * 1024 * 1024 + 1;
        fixture.setEntry(0, entry);
    }
    SUBCASE("entry compressed declaration above 512 MiB is metadata only") {
        entry.compressedSize = 512ull * 1024 * 1024 + 1;
        fixture.setEntry(0, entry);
    }
    fixture.seal();
    // Unchanged, consistent physical header plus re-encrypted index means
    // these cases pass the signature gate and reach index-entry validation.
    checkRejected(fixture);
}

TEST_CASE("U9: valid outer signature cannot rescue an invalid index authentication tag") {
    U9CarcFixture fixture;
    fixture.seal(true);
    checkRejected(fixture);
}

TEST_CASE("U9: authenticated entry failures return empty without poisoning another file") {
    U9CarcFixture fixture;
    auto entry = fixture.entry(0);
    REQUIRE(entry.compressedSize != entry.originalSize);
    SUBCASE("bad per-file GCM tag") { entry.tag[0] ^= 1; }
    SUBCASE("decoded output shorter than declared") { ++entry.originalSize; }
    SUBCASE("decoded output cannot fit declared size") { --entry.originalSize; }
    fixture.setEntry(0, entry);
    fixture.seal();
    CARCReader reader;
    REQUIRE(reader.open(fixture.path.string(), fixture.publicKey));
    CHECK(reader.hasFile("text.txt"));
    U9CarcFixture::Bytes failedRead;
    CHECK_NOTHROW(failedRead = reader.readFile("text.txt"));
    CHECK(failedRead.empty());
    CHECK(reader.readFile("noise.bin") == fixture.files[1].second);
    CHECK(reader.readFile("text.txt").empty());
}

TEST_CASE("U9: signed duplicate index keys preserve current bounded last-entry semantics") {
    U9CarcFixture fixture;
    auto second = fixture.entry(1);
    const auto first = fixture.entry(0);
    std::memcpy(second.pathHash, first.pathHash, PATH_HASH_SIZE);
    fixture.setEntry(1, second);
    fixture.seal();
    CARCReader reader;
    REQUIRE(reader.open(fixture.path.string(), fixture.publicKey));
    // Characterization, not a new rejection policy: the current raw list
    // counts entries while lookup uses the final entry for a repeated hash.
    CHECK(reader.numFiles() == 2);
    REQUIRE(reader.fileList().size() == 2);
    CHECK(reader.fileList()[0] == reader.fileList()[1]);
    CHECK(reader.index().size() == 1);
    CHECK(reader.readFile("text.txt") == fixture.files[1].second);
    CHECK_FALSE(reader.hasFile("noise.bin"));
    reader.close();
    checkClosed(reader);
}

TEST_CASE("U9: close reopen and a parser failure cannot retain an older index") {
    U9CarcFixture first;
    U9CarcFixture second(U9CarcFixture::Files{{"replacement.txt", U9CarcFixture::Bytes(1024, 'B')}});
    CARCReader reader;
    REQUIRE(reader.open(first.path.string(), first.publicKey));
    reader.close();
    reader.close();
    checkClosed(reader);
    REQUIRE(reader.open(second.path.string(), second.publicKey));
    CHECK_FALSE(reader.hasFile("text.txt"));
    CHECK(reader.readFile("replacement.txt") == second.files[0].second);
    auto entry = first.entry(1); // first entry parses before the second fails
    entry.offset = first.header.contentSize + 1;
    first.setEntry(1, entry);
    first.seal();
    CHECK_FALSE(reader.open(first.path.string(), first.publicKey));
    checkClosed(reader);
    CHECK_FALSE(reader.hasFile("replacement.txt"));
    REQUIRE(reader.open(second.path.string(), second.publicKey));
    CHECK(reader.readFile("replacement.txt") == second.files[0].second);
}

TEST_CASE("U9: an actual post-open short read clears cleanly for the next read") {
    U9CarcFixture fixture;
    CARCReader reader;
    REQUIRE(reader.open(fixture.path.string(), fixture.publicKey));
    // No worker accesses the stream during truncation/restoration. The noisy
    // file is larger than a normal stream buffer, preventing a cached full read.
    std::error_code error;
    std::filesystem::resize_file(fixture.path, sizeof(CARCHeader) + 1, error);
    REQUIRE_MESSAGE(!error, "fixture requires truncating its own open temporary file");
    U9CarcFixture::Bytes failedRead;
    CHECK_NOTHROW(failedRead = reader.readFile("noise.bin"));
    CHECK(failedRead.empty());
    fixture.writeBytes(); // restores the exact already-authenticated bytes
    CHECK(reader.readFile("noise.bin") == fixture.files[1].second);
    CHECK(reader.readFile("text.txt") == fixture.files[0].second);
}

TEST_CASE("U9: one reader supports concurrent exact reads and independent failures") {
    U9CarcFixture fixture(U9CarcFixture::Files{{"text.txt", U9CarcFixture::Bytes(8192, 'A')},
                           {"noise.bin", U9CarcFixture::noise(65537)},
                           {"bad.txt", U9CarcFixture::Bytes(1024, 'C')}});
    auto bad = fixture.entry(2);
    bad.tag[0] ^= 1;
    fixture.setEntry(2, bad);
    fixture.seal();
    CARCReader reader;
    REQUIRE(reader.open(fixture.path.string(), fixture.publicKey));
    std::atomic<bool> start{false};
    std::atomic<unsigned> failures{0};
    {
        // jthread ensures earlier workers join even if a later creation throws.
        // No close/open/verifySignature call overlaps the reader worker phase.
        std::vector<std::jthread> workers;
        workers.reserve(4);
        for (unsigned worker = 0; worker < 4; ++worker) {
            workers.emplace_back([&, worker](std::stop_token stop) {
                while (!start.load(std::memory_order_acquire)) {
                    if (stop.stop_requested()) return;
                    std::this_thread::yield();
                }
                try {
                    for (unsigned turn = 0; turn < 32; ++turn) {
                        const auto& file = fixture.files[(turn + worker) % 2];
                        if (reader.readFile(file.first) != file.second) ++failures;
                        if (!reader.readFile("bad.txt").empty()) ++failures;
                        if (!reader.readFile("missing.txt").empty()) ++failures;
                    }
                } catch (...) { ++failures; }
            });
        }
        start.store(true, std::memory_order_release);
    }
    CHECK(failures.load() == 0);
    reader.close();
    checkClosed(reader);
    REQUIRE(reader.open(fixture.path.string(), fixture.publicKey));
    CHECK(reader.readFile("text.txt") == fixture.files[0].second);
}
