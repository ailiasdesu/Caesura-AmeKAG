#pragma once

#include <bgfx/bgfx.h>
#include "IRenderDevice.h"
#include <unordered_map>
#include <cstdint>

namespace Caesura {

class BgfxDeviceCore {
public:
    BgfxDeviceCore() = default;
    ~BgfxDeviceCore();

    BgfxDeviceCore(const BgfxDeviceCore&) = delete;
    BgfxDeviceCore& operator=(const BgfxDeviceCore&) = delete;

    static constexpr uint16_t VIEW_RTT        = 0;
    static constexpr uint16_t VIEW_MAIN       = 1;
    static constexpr uint16_t VIEW_DEBUG      = 2;
    static constexpr uint16_t VIEW_TRANSITION = 3;

    static bool setPreferredBackend(const char* name);
    const char* getBackendName() const;

    bool init(void* nativeWindowHandle, int width, int height);
    void resize(int width, int height);
    void shutdown();
    void beginFrame();
    void endFrame();
    void commit_frame();
    void setViewRect(uint16_t v, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void setViewClear(uint16_t v, uint16_t f, uint32_t c, float d, uint8_t s);
    void touch(uint16_t v);
    void setDebugName(uint16_t v, const std::string& n);
    ViewportHandle createRenderTarget(int w, int h);
    void destroyRenderTarget(ViewportHandle h);
    bgfx::TextureHandle getViewportTexture(ViewportHandle h);
    bgfx::FrameBufferHandle getRttFb(ViewportHandle h);
    void flushAllRTT();
    int getWidth()  const { return m_width; }
    int getHeight() const { return m_height; }

private:
    void setupDefaultViews();
    int m_width  = 1280;
    int m_height = 720;
    struct RTTEntry { bgfx::FrameBufferHandle fb = BGFX_INVALID_HANDLE; bgfx::TextureHandle tex = BGFX_INVALID_HANDLE; uint16_t viewId = VIEW_RTT; };
    uint32_t m_nextHandle = 1;
    std::unordered_map<uint32_t, RTTEntry> m_rttMap;
};

} // namespace Caesura