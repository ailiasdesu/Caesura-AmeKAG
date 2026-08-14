// test_text_cache.cpp - GPU-free unit tests for TextRenderer::MessageLayerCache key semantics
// Covers P2-10: verify that the batch-cache is keyed on (viewId, text, x, y) as an
// ALL-parameters-match tuple, that dirty tracking behaves, that the CJK flag is
// orthogonal to the geometry key, and that empty text only matches empty.
//
// These tests construct MessageLayerCache directly (no GPU context required):
// the struct is a header-only value type whose matches()/isDirty()/markAllDirty()/
// clearDirty() members are pure logic, so they run headless just like the
// default-construction render tests in test_render_pipeline.cpp.
#include "doctest.h"
#include "render/TextRenderer.h"

#include <string>
#include <cstdint>

using namespace Caesura;

namespace {
// Helper: a cache configured to the task's canonical key tuple so deviations
// are a one-line change at each call site.
MessageLayerCache makeCanonicalCache()
{
    MessageLayerCache c;
    c.cachedText = "Hello";
    c.cachedViewId = 1;
    c.cachedX = 10.0f;
    c.cachedY = 20.0f;
    return c;
}

// -----------------------------------------------------------------------------
// Pure dirty-range math: computeDirtyRange() is extracted from
// updateDirtyRange() so the UTF-8-aware codepoint diff can be pinned without
// GPU state (P2-10 closure).
// -----------------------------------------------------------------------------

TEST_CASE("Text cache dirty range: identical text -> unchanged") {
    auto r = TextRenderer::computeDirtyRange("Hello", "Hello", 2048);
    CHECK_FALSE(r.changed);
    CHECK(r.start == 0);
    CHECK(r.end == 0);
}

TEST_CASE("Text cache dirty range: append marks only the new glyphs") {
    // "Hello" -> "Hello!" : shared prefix is all 5 glyphs, one new glyph.
    auto r = TextRenderer::computeDirtyRange("Hello", "Hello!", 2048);
    CHECK(r.changed);
    CHECK(r.start == 5);
    CHECK(r.end == 6);
}

TEST_CASE("Text cache dirty range: replace tail after shared prefix") {
    // "Hello World" -> "Hello DSH!": shared "Hello " (6 glyphs).
    // old remainder "World" (5), new remainder "DSH!" (4) -> max is 5.
    auto r = TextRenderer::computeDirtyRange("Hello World", "Hello DSH!", 2048);
    CHECK(r.changed);
    CHECK(r.start == 6);
    CHECK(r.end == 11);  // 6 + max(5,4)
}

TEST_CASE("Text cache dirty range: prepend dirties everything") {
    // "World" -> "Big World": no shared prefix (case differs at 'W' vs 'B'? no
    // -- 'W'=='W'? "Big World" starts with 'B', so prefix is empty).
    auto r = TextRenderer::computeDirtyRange("World", "Big World", 2048);
    CHECK(r.changed);
    CHECK(r.start == 0);
    CHECK(r.end == 9);  // max(5, 9)
}

TEST_CASE("Text cache dirty range: CJK codepoints never split") {
    // "中中" (2 glyphs) -> "中文" (2 glyphs): shared first glyph "中",
    // then diverge. Old remain "中" (1), new remain "文" (1).
    auto r = TextRenderer::computeDirtyRange(
        "\xE4\xB8\xAD\xE4\xB8\xAD",
        "\xE4\xB8\xAD\xE6\x96\x87", 2048);
    CHECK(r.changed);
    CHECK(r.start == 1);
    CHECK(r.end == 2);
}

TEST_CASE("Text cache dirty range: multi-byte prefix compare is byte-exact") {
    // Same bytes but different intended glyphs is impossible; here we pin
    // that a 3-byte CJK char does NOT match a 3-byte different char.
    // 中(0xE4B8AD) vs 文(0xE69687) at position 0 -> no shared prefix.
    auto r = TextRenderer::computeDirtyRange(
        "\xE4\xB8\xAD", "\xE6\x96\x87", 2048);
    CHECK(r.changed);
    CHECK(r.start == 0);
    CHECK(r.end == 1);
}

TEST_CASE("Text cache dirty range: clamps end to maxGlyphs") {
    // 10 glyphs differ but cache holds only 4.
    auto r = TextRenderer::computeDirtyRange("ABCDEFGHIJ", "abcdefghij", 4);
    CHECK(r.changed);
    CHECK(r.start == 0);
    CHECK(r.end == 4);
}

TEST_CASE("Text cache dirty range: empty text transitions") {
    // "" -> "x": append case.
    auto r1 = TextRenderer::computeDirtyRange("", "x", 2048);
    CHECK(r1.changed);
    CHECK(r1.start == 0);
    CHECK(r1.end == 1);

    // "x" -> "": clear case.
    auto r2 = TextRenderer::computeDirtyRange("x", "", 2048);
    CHECK(r2.changed);
    CHECK(r2.start == 0);
    CHECK(r2.end == 1);

    // "" -> "": identical.
    auto r3 = TextRenderer::computeDirtyRange("", "", 2048);
    CHECK_FALSE(r3.changed);
}

TEST_CASE("Text cache dirty range: countUtf8Glyphs lenient with truncation") {
    const uint8_t ascii[] = { 'A', 'B', 'C' };
    CHECK(TextRenderer::countUtf8Glyphs(ascii, 3) == 3);

    // 中 = 3 bytes, 文 = 3 bytes -> 2 glyphs.
    const uint8_t cjk[] = { 0xE4, 0xB8, 0xAD, 0xE6, 0x96, 0x87 };
    CHECK(TextRenderer::countUtf8Glyphs(cjk, 6) == 2);

    // Truncated trailing sequence: 中's bytes cut to 2 -> still 1 glyph.
    CHECK(TextRenderer::countUtf8Glyphs(cjk, 2) == 1);

    // 4-byte emoji (U+1F600) counts as one.
    const uint8_t emoji[] = { 0xF0, 0x9F, 0x98, 0x80 };
    CHECK(TextRenderer::countUtf8Glyphs(emoji, 4) == 1);

    // Empty input.
    CHECK(TextRenderer::countUtf8Glyphs(nullptr, 0) == 0);
}

TEST_CASE("Text cache dirty range: updateDirtyRange integration via cache struct") {
    // Drive the real member-updating path through the pure helper semantics:
    // simulate what updateDirtyRange does with a MessageLayerCache.
    MessageLayerCache c;
    c.cachedText = "Hello";
    c.maxGlyphs = 2048;
    const auto r = TextRenderer::computeDirtyRange(c.cachedText, "Hello!", c.maxGlyphs);
    if (r.changed) {
        c.dirtyStart = r.start;
        c.dirtyEnd = r.end;
        c.cachedText = "Hello!";
    }
    CHECK(c.isDirty());
    CHECK(c.dirtyStart == 5);
    CHECK(c.dirtyEnd == 6);
    CHECK(c.cachedText == "Hello!");
}

} // namespace

TEST_CASE("Text cache: matches requires ALL parameters equal") {
    MessageLayerCache c = makeCanonicalCache();

    // Exactly equal -> hit.
    CHECK(c.matches(1, "Hello", 10.0f, 20.0f));

    // Deviating any single parameter must miss.
    CHECK_FALSE(c.matches(1, "World", 10.0f, 20.0f)); // different text
    CHECK_FALSE(c.matches(2, "Hello", 10.0f, 20.0f)); // different view
    CHECK_FALSE(c.matches(1, "Hello", 11.0f, 20.0f)); // different x
    CHECK_FALSE(c.matches(1, "Hello", 10.0f, 21.0f)); // different y

    // Two parameters deviating together also miss (sanity).
    CHECK_FALSE(c.matches(2, "World", 10.0f, 20.0f));
}

TEST_CASE("Text cache: dirty tracking") {
    MessageLayerCache c;
    c.cachedText = "Hello";
    c.cachedViewId = 1;

    // Fresh cache: nothing dirty.
    CHECK_FALSE(c.isDirty());
    CHECK(c.dirtyStart == c.dirtyEnd);

    c.markAllDirty();
    CHECK(c.isDirty());
    CHECK(c.dirtyStart == 0);
    CHECK(c.dirtyEnd == c.maxGlyphs); // full range covered

    c.clearDirty();
    CHECK_FALSE(c.isDirty());
    CHECK(c.dirtyStart == c.dirtyEnd);
    CHECK(c.dirtyStart == 0);
}

TEST_CASE("Text cache: CJK flag independent of geometry key") {
    MessageLayerCache c = makeCanonicalCache();

    // Setting the CJK flag must not perturb the cache-hit decision.
    c.cacheIsCjk = true;
    CHECK(c.matches(1, "Hello", 10.0f, 20.0f));

    c.cacheIsCjk = false;
    CHECK(c.matches(1, "Hello", 10.0f, 20.0f));

    // A single key deviation still misses regardless of the CJK flag.
    c.cacheIsCjk = true;
    CHECK_FALSE(c.matches(1, "World", 10.0f, 20.0f));
}

TEST_CASE("Text cache: empty text matches only empty") {
    MessageLayerCache c;
    c.cachedText = "";
    c.cachedViewId = 1;
    c.cachedX = 10.0f;
    c.cachedY = 20.0f;

    CHECK(c.matches(1, "", 10.0f, 20.0f));      // empty == empty
    CHECK_FALSE(c.matches(1, "x", 10.0f, 20.0f)); // "x" != ""

    // Empty text is still geometry-sensitive: a differing y must miss.
    CHECK_FALSE(c.matches(1, "", 10.0f, 21.0f));
}