#include "NullPlatformBackend.h"

#include <cstdio>

namespace Caesura {

NullPlatformBackend::NullPlatformBackend() {
    std::printf("[Platform] Using NullPlatformBackend.\n");
}

bool NullPlatformBackend::init(const char*, int width, int height) {
    m_width = width;
    m_height = height;
    return true;
}

void NullPlatformBackend::shutdown() {}
bool NullPlatformBackend::pollEvent() { return false; }
IPlatformBackend::MouseState NullPlatformBackend::getMouseState() const { return {}; }
uint64_t NullPlatformBackend::getTicksMs() const { return 0; }
void* NullPlatformBackend::getNativeWindowHandle() const { return nullptr; }
int NullPlatformBackend::getWindowWidth() const { return m_width; }
int NullPlatformBackend::getWindowHeight() const { return m_height; }
void NullPlatformBackend::setFullscreen(bool) {}
void NullPlatformBackend::resizeWindow(int width, int height) {
    m_width = width;
    m_height = height;
}
const char* NullPlatformBackend::getBackendName() const { return "NullPlatform"; }

} // namespace Caesura
