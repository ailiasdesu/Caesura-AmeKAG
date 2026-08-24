#pragma once

#include "api/IPlatformBackend.h"
#include <cstdint>  // fixed-width types (GCC strict)

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

    // -- Text Input / IME (Virtual Keyboard) --------------------------------
    bool startTextInput() override;
    bool stopTextInput() override;
    bool setTextInputRect(int x, int y, int w, int h, int cursor = 0) override;
    bool isTextInputActive() const override;

private:
    int m_width = 0;
    int m_height = 0;
    bool m_initialized = false;
    bool m_textInputActive = false;
    int m_textInputX = 0;
    int m_textInputY = 0;
    int m_textInputW = 0;
    int m_textInputH = 0;
    int m_textInputCursor = 0;
};

} // namespace Caesura
