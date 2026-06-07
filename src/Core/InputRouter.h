#pragma once
#include <SDL3/SDL.h>
#include <functional>
#include <string>
#include <vector>

namespace Caesura {

// -- Input Focus -----------------------------------------------------------
enum class InputFocus {
    KAG,   // KAG script input: clicks/keys advance story
    GAME   // Game mode: input dispatched to custom game callbacks
};

// -- Input Router ----------------------------------------------------------
// Global input focus switch. Routes SDL events to either the KAG narrative
// system or a registered GAME-mode callback based on the current focus.

using GameInputCallback = std::function<void(const SDL_Event&)>;
using FocusChangeCallback = std::function<void(InputFocus newFocus)>;
// Window resize notification -- spec [0.1]: notify layers to rebuild, mark dirty
using ResizeCallback = std::function<void(int newWidth, int newHeight)>;

class InputRouter {
public:
    InputRouter() = default;

    // Process an SDL event; routes according to current focus.
    void processEvent(const SDL_Event& event);

    // Set the input focus mode.
    void setFocus(InputFocus focus);

    // Get current focus.
    InputFocus getFocus() const { return m_focus; }

    // Register a callback for GAME-mode input.
    void registerGameCallback(GameInputCallback cb);

    // Register a callback for KAG-mode input (usually pushes to Lua).
    void registerKAGCallback(GameInputCallback cb);

    // Register a callback triggered on focus change.
    void registerFocusChangeCallback(FocusChangeCallback cb);

    // Register a callback for window resize events.
    // Called by Engine::processEvents() when SDL_EVENT_WINDOW_RESIZED fires.
    // Lua-side layers.lua rebuilds the layer tree and marks all layers dirty.
    void registerResizeCallback(ResizeCallback cb);

    // Notify all registered resize callbacks (called from Engine).
    void notifyResize(int newWidth, int newHeight);


    // Check if a KAG-input click event is pending (for wait_click).
    bool hasKAGClick() const { return m_kagClickPending; }
    // Callback counts for diagnostics
    size_t getKAGCallbackCount() const  { return m_kagCallbacks.size(); }
    size_t getGameCallbackCount() const { return m_gameCallbacks.size(); }
    // Alias for hasKAGClick (for consistency)
    bool isClickPending() const { return m_kagClickPending; }

    // Consume a pending KAG click.
    void consumeKAGClick() { m_kagClickPending = false; }

private:
    InputFocus m_focus = InputFocus::KAG;
    bool m_kagClickPending = false;

    std::vector<GameInputCallback> m_gameCallbacks;
    std::vector<GameInputCallback> m_kagCallbacks;
    std::vector<FocusChangeCallback> m_focusChangeCallbacks;
    std::vector<ResizeCallback> m_resizeCallbacks;
};

// -- Helper ----------------------------------------------------------------
const char* inputFocusToString(InputFocus focus);

} // namespace Caesura
