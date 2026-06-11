#ifdef CAESURA_HAS_LIVE2D

#include "MetalNativeRenderPath.h"
#include <SDL3/SDL.h>

namespace Caesura {

bool MetalNativeRenderPath::init(int, int) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
        "[Live2D/Metal] STUB — Cubism Metal renderer not yet verified for macOS. "
        "Falling back to OpenGL readback. macOS developer needed.");
    return false;
}

void MetalNativeRenderPath::shutdown() {}

CsmRendering::CubismRenderer* MetalNativeRenderPath::createRenderer() { return nullptr; }

void MetalNativeRenderPath::beginFrame(CsmRendering::CubismRenderer*) {}
void MetalNativeRenderPath::endFrame(CsmRendering::CubismRenderer*, bgfx::TextureHandle) {}
void MetalNativeRenderPath::resize(int, int) {}

} // namespace Caesura

#endif