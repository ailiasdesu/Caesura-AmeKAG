// NullRenderDevice — no-op implementation of IRenderDevice for testing
#pragma once
#include "IRenderDevice.h"

namespace Caesura {

class NullRenderDevice : public IRenderDevice {
public:
    bool init(void*, int, int) override { return true; }
    void shutdown() override {}
    void flushAllRTT() override {}
    void beginFrame() override {}
    void endFrame() override {}
    void commit_frame() override {}
    void touch(uint16_t) override {}
    ViewportHandle createRenderTarget(int, int) override { return {}; }
    void destroyRenderTarget(ViewportHandle) override {}
    bgfx::TextureHandle getViewportTexture(ViewportHandle) override { return BGFX_INVALID_HANDLE; }
    int getBackbufferWidth() const override { return 0; }
    int getBackbufferHeight() const override { return 0; }
    void resize(int, int) override {}
    void beginBatch() override {}
    void flushBatch() override {}
    void setDebugName(uint16_t, const std::string&) override {}
    void setFont(int) override {}
    float textLineHeight() const override { return 0; }
    void fillViewport(ViewportHandle, uint8_t, uint8_t, uint8_t, uint8_t) override {}
};

} // namespace Caesura
