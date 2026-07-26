#pragma once
#include <SDL3/SDL.h>
#include <functional>
#include <cstddef>

namespace Caesura {

enum class InputFocus {
    KAG,
    GAME
};

using GameInputCallback = std::function<void(const SDL_Event&)>;
using FocusChangeCallback = std::function<void(InputFocus newFocus)>;
using ResizeCallback = std::function<void(int newWidth, int newHeight)>;

// ============================================================================
// IInputRouter — pure virtual interface for input routing
// ============================================================================
// InputRouter implements this interface. BackendRegistry stores IInputRouter*.

class IInputRouter {
public:
    virtual ~IInputRouter() = default;

    virtual void processEvent(const SDL_Event& event) = 0;
    virtual void setFocus(InputFocus focus) = 0;
    virtual InputFocus getFocus() const = 0;

    virtual void registerGameCallback(GameInputCallback cb) = 0;
    virtual void registerKAGCallback(GameInputCallback cb) = 0;
    virtual void registerFocusChangeCallback(FocusChangeCallback cb) = 0;
    virtual void registerResizeCallback(ResizeCallback cb) = 0;
    virtual void notifyResize(int newWidth, int newHeight) = 0;

    virtual bool hasKAGClick() const = 0;
    virtual size_t getKAGCallbackCount() const = 0;
    virtual size_t getGameCallbackCount() const = 0;
    virtual bool isClickPending() const = 0;
    virtual void consumeKAGClick() = 0;
};

} // namespace Caesura
