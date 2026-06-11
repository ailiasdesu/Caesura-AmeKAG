#include "SDL3PlatformBackend.h"
#include <cstdio>

namespace Caesura {

SDL3PlatformBackend::~SDL3PlatformBackend() {
    shutdown();
}

bool SDL3PlatformBackend::init(const char* title, int width, int height) {
    if (m_initialized) return true;

    m_width  = width;
    m_height = height;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "[SDL3] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);

    m_window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);

    if (!m_window) {
        fprintf(stderr, "[SDL3] CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_ShowWindow(m_window);
    m_initialized = true;
    printf("[SDL3] Platform backend initialized: %dx%d \"%s\"\n", width, height, title);
    return true;
}

void SDL3PlatformBackend::shutdown() {
    if (!m_initialized) return;
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
    m_initialized = false;
    printf("[SDL3] Platform backend shut down.\n");
}

bool SDL3PlatformBackend::pollEvent() {
    if (!m_initialized) return false;
    return SDL_PollEvent(&m_lastEvent);
}

IPlatformBackend::MouseState SDL3PlatformBackend::getMouseState() const {
    MouseState ms;
    float mx, my;
    SDL_GetMouseState(&mx, &my);
    ms.x = mx; ms.y = my;
    ms.leftDown = (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0;
    return ms;
}

uint64_t SDL3PlatformBackend::getTicksMs() const {
    return SDL_GetTicks();
}

void SDL3PlatformBackend::resizeWindow(int width, int height) {
    if (m_window) {
        SDL_SetWindowSize(m_window, width, height);
        m_width = width;
        m_height = height;
    }
}

void SDL3PlatformBackend::setFullscreen(bool fullscreen) {
    if (m_window) {
        SDL_SetWindowFullscreen(m_window, fullscreen);
        printf("[SDL3] Fullscreen: %s\n", fullscreen ? "on" : "off");
    }
}

void* SDL3PlatformBackend::getNativeWindowHandle() const {
    if (!m_window) return nullptr;

    SDL_PropertiesID props = SDL_GetWindowProperties(m_window);

    void* nwh = SDL_GetPointerProperty(props,
        SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    if (!nwh) {
        nwh = SDL_GetPointerProperty(props,
            SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    }
    if (!nwh) {
        nwh = (void*)(uintptr_t)SDL_GetNumberProperty(props,
            SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    }
    if (!nwh) {
        nwh = SDL_GetPointerProperty(props,
            SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    }

    return nwh;
}

} // namespace Caesura