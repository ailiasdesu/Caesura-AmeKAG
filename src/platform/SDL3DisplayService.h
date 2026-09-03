#pragma once

#include "api/IDisplayService.h"

struct SDL_Window;

namespace Caesura {

// Desktop display metrics queried lazily from the SDL window owned by
// SDL3PlatformBackend. The bgfx-native handle
// is deliberately not used here: it may be an HWND, NSWindow, X11 Window, or
// ANativeWindow rather than an SDL_Window.
class SDL3DisplayService final : public IDisplayService {
public:
    explicit SDL3DisplayService(SDL_Window* window)
        : m_window(window) {}

    DisplayMetrics currentMetrics() const override;

private:
    SDL_Window* m_window = nullptr;
};

} // namespace Caesura
