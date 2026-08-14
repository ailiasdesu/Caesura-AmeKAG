// =============================================================================
// test_quad_batch.cpp — GPU-free unit tests for BgfxQuadBatch's pure batch
// math (P2-10 closure).
//
// quadToNdc / computeMergeGroups / buildGroupIndices were extracted from
// flushBatch() so the NDC conversion, merge-group splitting and local index
// generation can be pinned without a GPU. BatchQuad only carries plain
// values (viewId, texture handle idx, rect, opacity) so tests construct them
// directly; no bgfx call is ever made.
// =============================================================================

#include "doctest.h"

#include "render/BgfxDraw.h"
#include "render/BgfxQuadBatch.h"

#include <vector>
#include <cstdint>

using namespace Caesura;

namespace {

BatchQuad makeQuad(uint16_t viewId, uint16_t texIdx, uint8_t opacity,
                   float x = 0.0f, float y = 0.0f, float w = 10.0f, float h = 10.0f) {
    BatchQuad q;
    q.viewId = viewId;
    q.tex.idx = texIdx;
    q.x = x; q.y = y; q.w = w; q.h = h;
    q.opacity = opacity;
    return q;
}

} // namespace

TEST_CASE("Quad batch: quadToNdc converts pixel rect to clip space") {
    // Center of a 640x360 screen -> (0,0); 10x10 quad around it.
    auto n = BgfxQuadBatch::quadToNdc(315.0f, 175.0f, 10.0f, 10.0f, 640.0f, 360.0f);
    CHECK(n.nx0 == doctest::Approx(315.0f / 640.0f * 2.0f - 1.0f));
    CHECK(n.ny0 == doctest::Approx(1.0f - 175.0f / 360.0f * 2.0f));
    CHECK(n.nx1 == doctest::Approx(325.0f / 640.0f * 2.0f - 1.0f));
    CHECK(n.ny1 == doctest::Approx(1.0f - 185.0f / 360.0f * 2.0f));

    // Screen corners: (0,0) -> (-1, 1); (sw,sh) -> (1,-1).
    auto tl = BgfxQuadBatch::quadToNdc(0.0f, 0.0f, 1.0f, 1.0f, 640.0f, 360.0f);
    CHECK(tl.nx0 == doctest::Approx(-1.0f));
    CHECK(tl.ny0 == doctest::Approx(1.0f));
    auto br = BgfxQuadBatch::quadToNdc(639.0f, 359.0f, 1.0f, 1.0f, 640.0f, 360.0f);
    CHECK(br.nx1 == doctest::Approx(1.0f));
    CHECK(br.ny1 == doctest::Approx(-1.0f));
}

TEST_CASE("Quad batch: all quads merge when tex/view/opacity match") {
    std::vector<BatchQuad> quads = {
        makeQuad(1, 7, 255),
        makeQuad(1, 7, 255),
        makeQuad(1, 7, 255),
    };
    std::vector<BgfxQuadBatch::MergeGroup> groups;
    BgfxQuadBatch::computeMergeGroups(quads, groups);
    REQUIRE(groups.size() == 1);
    CHECK(groups[0].startQuad == 0);
    CHECK(groups[0].quadCount == 3);
}

TEST_CASE("Quad batch: texture change splits groups") {
    std::vector<BatchQuad> quads = {
        makeQuad(1, 7, 255),
        makeQuad(1, 8, 255),   // different texture
        makeQuad(1, 7, 255),   // back to tex 7 -- NOT merged with first group
    };
    std::vector<BgfxQuadBatch::MergeGroup> groups;
    BgfxQuadBatch::computeMergeGroups(quads, groups);
    REQUIRE(groups.size() == 3);
    CHECK(groups[0].quadCount == 1);
    CHECK(groups[1].quadCount == 1);
    CHECK(groups[2].quadCount == 1);
}

TEST_CASE("Quad batch: view change splits groups") {
    std::vector<BatchQuad> quads = {
        makeQuad(1, 7, 255),
        makeQuad(2, 7, 255),   // different view
        makeQuad(1, 7, 255),
    };
    std::vector<BgfxQuadBatch::MergeGroup> groups;
    BgfxQuadBatch::computeMergeGroups(quads, groups);
    REQUIRE(groups.size() == 3);
}

TEST_CASE("Quad batch: opacity change splits groups") {
    std::vector<BatchQuad> quads = {
        makeQuad(1, 7, 255),
        makeQuad(1, 7, 128),   // different opacity
        makeQuad(1, 7, 255),
    };
    std::vector<BgfxQuadBatch::MergeGroup> groups;
    BgfxQuadBatch::computeMergeGroups(quads, groups);
    REQUIRE(groups.size() == 3);
}

TEST_CASE("Quad batch: mixed merge pattern with accurate boundaries") {
    // tex A A B B B A view1 all, opacity varies at index 3
    std::vector<BatchQuad> quads = {
        makeQuad(1, 7, 255),
        makeQuad(1, 7, 255),
        makeQuad(1, 8, 255),
        makeQuad(1, 8, 128),   // opacity breaks the B run
        makeQuad(1, 8, 128),
        makeQuad(1, 7, 255),
    };
    std::vector<BgfxQuadBatch::MergeGroup> groups;
    BgfxQuadBatch::computeMergeGroups(quads, groups);
    REQUIRE(groups.size() == 4);
    CHECK(groups[0].startQuad == 0);
    CHECK(groups[0].quadCount == 2); // A
    CHECK(groups[1].startQuad == 2);
    CHECK(groups[1].quadCount == 1); // B/255
    CHECK(groups[2].startQuad == 3);
    CHECK(groups[2].quadCount == 2); // B/128
    CHECK(groups[3].startQuad == 5);
    CHECK(groups[3].quadCount == 1); // A
}

TEST_CASE("Quad batch: empty quads produce no groups") {
    std::vector<BatchQuad> quads;
    std::vector<BgfxQuadBatch::MergeGroup> groups;
    BgfxQuadBatch::computeMergeGroups(quads, groups);
    CHECK(groups.empty());
}

TEST_CASE("Quad batch: single quad is one group") {
    std::vector<BatchQuad> quads = { makeQuad(3, 9, 200) };
    std::vector<BgfxQuadBatch::MergeGroup> groups;
    BgfxQuadBatch::computeMergeGroups(quads, groups);
    REQUIRE(groups.size() == 1);
    CHECK(groups[0].startQuad == 0);
    CHECK(groups[0].quadCount == 1);
}

TEST_CASE("Quad batch: buildGroupIndices emits 6 rebased indices per quad") {
    std::vector<uint16_t> indices;
    BgfxQuadBatch::buildGroupIndices(2, indices);
    REQUIRE(indices.size() == 12);
    // Quad 0: base 0 -> (0,1,2, 0,2,3)
    CHECK(indices[0] == 0); CHECK(indices[1] == 1); CHECK(indices[2] == 2);
    CHECK(indices[3] == 0); CHECK(indices[4] == 2); CHECK(indices[5] == 3);
    // Quad 1: base 4 -> (4,5,6, 4,6,7)
    CHECK(indices[6] == 4); CHECK(indices[7] == 5); CHECK(indices[8] == 6);
    CHECK(indices[9] == 4); CHECK(indices[10] == 6); CHECK(indices[11] == 7);
}

TEST_CASE("Quad batch: buildGroupIndices single quad and empty") {
    std::vector<uint16_t> indices;
    BgfxQuadBatch::buildGroupIndices(1, indices);
    REQUIRE(indices.size() == 6);
    CHECK(indices[0] == 0); CHECK(indices[1] == 1); CHECK(indices[2] == 2);
    CHECK(indices[3] == 0); CHECK(indices[4] == 2); CHECK(indices[5] == 3);

    BgfxQuadBatch::buildGroupIndices(0, indices);
    CHECK(indices.empty());
}

TEST_CASE("Quad batch: group coverage is complete and contiguous") {
    // Property check: for any quad list, groups must tile [0, quadCount)
    // exactly with no overlap.
    std::vector<BatchQuad> quads = {
        makeQuad(1, 1, 255), makeQuad(1, 1, 255),
        makeQuad(2, 1, 255), makeQuad(2, 2, 255),
        makeQuad(2, 2, 255), makeQuad(2, 2, 100),
        makeQuad(1, 3, 255),
    };
    std::vector<BgfxQuadBatch::MergeGroup> groups;
    BgfxQuadBatch::computeMergeGroups(quads, groups);
    uint32_t cursor = 0;
    for (const auto& g : groups) {
        CHECK(g.startQuad == cursor);
        cursor += g.quadCount;
    }
    CHECK(cursor == quads.size());
}