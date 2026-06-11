#pragma once

#include <bgfx/bgfx.h>
#include <unordered_map>
#include <cstdint>

namespace Caesura {

class BgfxShaderManager;

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

    bool init(void* nativeWindowHandle, int width, int height, BgfxShaderManager* shaders);
    void resize(int width, int height);
    void shutdown();

    void beginFrame();
    void endFrame();
    void commit_frame();

    void setViewRect(uint16_t viewId, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void setViewClear(uint16_t viewId, uint16_t flags, uint32_t rgba, float depth, uint8_t stencil);
    void touch(uint16_t viewId);
    void setDebugName(uint16_t viewId, const char* name);

    ViewportHandle createRenderTarget(int width, int height);
    void destroyRenderTarget(ViewportHandle handle);
    bgfx::TextureHandle getViewportTexture(ViewportHandle handle);
    bgfx::FrameBufferHandle getRttFb(ViewportHandle handle);
    void flushAllRTT();

    int getWidth()  const { return m_width; }
    int getHeight() const { return m_height; }

private:
    void setupDefaultViews(BgfxShaderManager* shaders);

    int m_width  = 1280;
    int m_height = 720;

    struct RTTEntry {
        bgfx::FrameBufferHandle fb    = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle     tex   = BGFX_INVALID_HANDLE;
        uint16_t                viewId = VIEW_RTT;
    };
    uint32_t m_nextHandle = 1;
    std::unordered_map<uint32_t, RTTEntry> m_rttMap;
};

} // namespace Caesura