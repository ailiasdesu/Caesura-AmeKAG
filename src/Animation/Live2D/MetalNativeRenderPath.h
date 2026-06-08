#pragma once
#ifdef CAESURA_HAS_LIVE2D

#include "ILive2DRenderPath.h"

namespace Caesura {

// macOS/iOS stub: Cubism Metal renderer exists in Framework but unverified for macOS.
// Requires macOS developer to validate and implement.
// Falls back to OpenGLReadbackRenderPath until then.
class MetalNativeRenderPath : public ILive2DRenderPath {
public:
    bool init(int width, int height) override;
    void shutdown() override;
    CsmRendering::CubismRenderer* createRenderer() override;
    void beginFrame(CsmRendering::CubismRenderer* renderer) override;
    void endFrame(CsmRendering::CubismRenderer* renderer, bgfx::TextureHandle bgfxTex) override;
    void resize(int width, int height) override;
    const char* name() const override { return "MetalNative(STUB)"; }
};

} // namespace Caesura

#endif