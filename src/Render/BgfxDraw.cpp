#include "BgfxDraw.h"
#include "BgfxShaderManager.h"
#include "BgfxDeviceCore.h"
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <cstdio>
#include <cstring>

namespace Caesura {

void BgfxDraw::init(DrawState* state) { m_state = state; }

} // namespace Caesura
