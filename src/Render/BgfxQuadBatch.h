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

private:
    DrawState* m_state = nullptr;
};

} // namespace Caesura