#pragma once

#include "api/IPlatformBackend.h"

namespace Caesura {

class NullPlatformBackend final : public IPlatformBackend {
public:
    NullPlatformBackend();

    bool init(const char* title, int width, int height) override;
    void shutdown() override;
    bool pollEvent() override;
    MouseState getMouseState() const override;
    uint64_t getTicksMs() const override;
    void* getNativeWindowHandle() const override;
    int getWindowWidth() const override;
    int getWindowHeight() const override;
    void setFullscreen(bool fullscreen) override;
    void resizeWindow(int width, int height) override;
    const char* getBackendName() const override;

private:
    int m_width = 0;
    int m_height = 0;
};

} // namespace Caesura
