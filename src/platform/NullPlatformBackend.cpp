#include "NullPlatformBackend.h"

#include <cstdio>

namespace Caesura {

NullPlatformBackend::NullPlatformBackend() {
    std::printf("[Platform] Using NullPlatformBackend.\n");
}

bool NullPlatformBackend::init(const char*, int width, int height) {
    m_width = width;
    m_height = height;
    m_initialized = true;
    return true;
}

void NullPlatformBackend::shutdown() {
    m_initialized = false;
    m_textInputActive = false;
}
bool NullPlatformBackend::pollEvent() { return false; }
IPlatformBackend::MouseState NullPlatformBackend::getMouseState() const { return {}; }
uint64_t NullPlatformBackend::getTicksMs() const { return 0; }
void* NullPlatformBackend::getNativeWindowHandle() const { return nullptr; }
int NullPlatformBackend::getWindowWidth() const { return m_width; }
int NullPlatformBackend::getWindowHeight() const { return m_height; }
void NullPlatformBackend::setFullscreen(bool) {}
void NullPlatformBackend::resizeWindow(int width, int height) {
    // Gate on init state to match SDL3PlatformBackend::resizeWindow, which no-ops
    // until a real window exists. A pre-init or post-shutdown resize is a no-op.
    if (!m_initialized) return;
    m_width = width;
    m_height = height;
}
const char* NullPlatformBackend::getBackendName() const { return "NullPlatform"; }

bool NullPlatformBackend::startTextInput() {
    if (!m_initialized) return false;
    m_textInputActive = true;
    return true;
}

bool NullPlatformBackend::stopTextInput() {
    m_textInputActive = false;
    return true;
}

bool NullPlatformBackend::setTextInputRect(int x, int y, int w, int h, int cursor) {
    if (!m_initialized) return false;
    m_textInputX = x;
    m_textInputY = y;
    m_textInputW = w;
    m_textInputH = h;
    m_textInputCursor = cursor;
    return true;
}

bool NullPlatformBackend::isTextInputActive() const {
    return m_initialized && m_textInputActive;
}

} // namespace Caesura
