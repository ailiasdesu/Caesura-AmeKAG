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
