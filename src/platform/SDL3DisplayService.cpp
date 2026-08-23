#include "SDL3DisplayService.h"

#include "api/IPlatformBackend.h"

#include <SDL3/SDL.h>

namespace Caesura {

DisplayMetrics SDL3DisplayService::currentMetrics() const {
    DisplayMetrics m;
    if (!m_platform) return m;

    SDL_Window* window = static_cast<SDL_Window*>(m_platform->getNativeWindowHandle());
    if (!window) return m; // headless / not yet created

    int w = 0, h = 0;
    if (SDL_GetWindowSize(window, &w, &h) && w > 0 && h > 0) {
        m.logicalWidth = static_cast<uint32_t>(w);
        m.logicalHeight = static_cast<uint32_t>(h);
    }
    int pw = 0, ph = 0;
    if (SDL_GetWindowSizeInPixels(window, &pw, &ph) && pw > 0 && ph > 0) {
        m.pixelWidth = static_cast<uint32_t>(pw);
        m.pixelHeight = static_cast<uint32_t>(ph);
    }
    // Fall back to logical size when the pixel query is unavailable/same.
    if (m.pixelWidth == 0) m.pixelWidth = m.logicalWidth;
    if (m.pixelHeight == 0) m.pixelHeight = m.logicalHeight;

    // Content scale (1.0 / 1.25 / 2.0 ...). SDL3 3.2 exposes content scale,
    // not raw DPI; dpi is documented as 96 * scale.
    double scale = 1.0;
    const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (display != 0) {
        const float cs = SDL_GetDisplayContentScale(display);
        if (cs > 0.0f) scale = static_cast<double>(cs);
    }
    m.scaleFactor = scale;
    m.dpi = 96.0 * scale;

    if (display != 0) {
        switch (SDL_GetCurrentDisplayOrientation(display)) {
            case SDL_ORIENTATION_PORTRAIT:        m.orientation = Orientation::Portrait; break;
            case SDL_ORIENTATION_PORTRAIT_FLIPPED: m.orientation = Orientation::PortraitUpsideDown; break;
            case SDL_ORIENTATION_LANDSCAPE:       m.orientation = Orientation::LandscapeLeft; break;
            case SDL_ORIENTATION_LANDSCAPE_FLIPPED: m.orientation = Orientation::LandscapeRight; break;
            case SDL_ORIENTATION_UNKNOWN:         m.orientation = Orientation::Unknown; break;
        }
    }
    // Desktop has no notch/rounded-corner insets.
    m.safeArea = Insets{};
    return m;
}

} // namespace Caesura
