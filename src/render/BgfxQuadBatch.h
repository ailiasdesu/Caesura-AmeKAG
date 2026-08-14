#pragma once

#include <bgfx/bgfx.h>
#include <vector>
#include <cstdint>

namespace Caesura {

struct BatchQuad;
class BgfxShaderManager;

struct DrawState;

class BgfxQuadBatch {
public:
    BgfxQuadBatch() = default;
    ~BgfxQuadBatch() = default;

    void init(DrawState* state) { m_state = state; }
    void beginBatch();
    void flushBatch();

    // -- Pure batch math (headless-testable) ------------------------------
    // Extracted from flushBatch() so the NDC conversion, merge-group
    // computation and index generation can be pinned without a GPU (P2-10).
    struct NdcRect { float nx0, ny0, nx1, ny1; };
    // Pixel -> NDC for a quad (matches the passthrough fallback VS).
    static NdcRect quadToNdc(float x, float y, float w, float h,
                             float screenW, float screenH);
    // One maximal contiguous run of quads sharing (tex, viewId, opacity).
    struct MergeGroup { uint32_t startQuad; uint32_t quadCount; };
    // Split quads into maximal runs that can share a single submit.
    // GPU-free: compares handle idx values only, never touches bgfx.
    static void computeMergeGroups(const std::vector<BatchQuad>& quads,
                                   std::vector<MergeGroup>& groups);
    // Local (rebase-from-0) 6-index-per-quad list for one group of
    // quadCount quads; values stay within [0, quadCount*4).
    static void buildGroupIndices(uint32_t quadCount,
                                  std::vector<uint16_t>& indices);

private:
    DrawState* m_state = nullptr;
};

} // namespace Caesura