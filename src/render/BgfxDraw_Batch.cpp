// BgfxDraw_Batch.cpp - Batch management (init, beginBatch, flushBatch)
#include "BgfxDraw.h"
#include "BgfxQuadBatch.h"
#include <cstdio>

namespace Caesura {

void BgfxDraw::init(DrawState* state) {
    m_state = state;
    m_quadBatch = std::make_unique<BgfxQuadBatch>();
    m_quadBatch->init(m_state);
}

void BgfxDraw::beginBatch() {
    m_quadBatch->beginBatch();
}

void BgfxDraw::flushBatch() {
    m_quadBatch->flushBatch();
}

} // namespace Caesura