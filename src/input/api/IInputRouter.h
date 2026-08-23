#pragma once
#include <SDL3/SDL.h>
#include <functional>
#include <cstddef>

namespace Caesura {

enum class InputFocus {
    KAG,
    GAME
};

// Unified pointer input (Track P3): platform-agnostic abstraction so the
// router (and KAG/UI consumers) do not depend on SDL_Event. Native touch
// sources (Android JNI / iOS) submit PointerEvent; the legacy SDL
// touch->mouse injection (MobileAdapter) stays as a compatibility path.
enum class PointerAction : uint8_t {
    Down,       // first contact
    Move,       // contact moved
    Up,         // contact released
    LongPress,  // contact held > typical threshold (500ms)
    Pinch,      // two-contact scale delta (scale is cumulative)
};

struct PointerEvent {
    PointerAction action = PointerAction::Move;
    float x = 0.0f;             // window logical pixels
    float y = 0.0f;
    float scale = 1.0f;         // pinch cumulative scale (1 = baseline)
    int32_t pointerId = 0;      // multi-touch finger id
    size_t activePointers = 1;  // concurrent contacts (multi-touch info)
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

    // Unified pointer path (Track P3): same KAG/GAME routing + phantom-click
    // guarantees as processEvent, but source-agnostic. Down/Move/Up map to
    // the mouse-equivalent semantics; LongPress -> right-button press/release;
    // Pinch -> wheel delta derived from the cumulative scale.
    virtual void submitPointer(const PointerEvent& event) = 0;
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
