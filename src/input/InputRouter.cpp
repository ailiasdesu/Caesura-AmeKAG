#include "InputRouter.h"
#include <cstdio>

namespace Caesura {

namespace {

bool isValidFocus(InputFocus focus) {
    return focus == InputFocus::KAG || focus == InputFocus::GAME;
}

} // namespace

const char* inputFocusToString(InputFocus focus) {
    switch (focus) {
        case InputFocus::KAG:  return "KAG";
        case InputFocus::GAME: return "GAME";
    }
    return "UNKNOWN";
}

// -------------------------------------------------------------------------------
//  InputRouter::processEvent   single-event dispatch
// -------------------------------------------------------------------------------
//
//  Routing contract:
//    KAG  mode   only KAG callbacks receive events; sets kagClickPending flag
//    GAME mode   only GAME callbacks receive events; KAG flag stays cleared
//
//  Boundary guarantee:
//    When focus == GAME, NO event ever reaches KAG callbacks.
//    When focus == KAG, NO event ever reaches GAME callbacks.
//    The two callback chains are mutually exclusive at the dispatch level.
// -------------------------------------------------------------------------------

void InputRouter::processEvent(const SDL_Event& event) {
    dispatchSdlEvent(event);
}

// Single dispatch point: processEvent and the unified pointer path (P3)
// funnel through this so routing/phantom-click guarantees stay identical.
void InputRouter::dispatchSdlEvent(const SDL_Event& event) {
    switch (m_focus) {
        case InputFocus::KAG: {
            // In KAG mode, mouse clicks and keyboard advance the story
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                event.type == SDL_EVENT_KEY_DOWN) {
                // Publish before callbacks so a handler can consume this input.
                m_kagClickPending = true;

                for (auto& cb : m_kagCallbacks) {
                    if (m_focus != InputFocus::KAG) break;
                    cb(event);
                }
            }
            break;
        }
        case InputFocus::GAME: {
            // In GAME mode, dispatch ALL events to game callbacks.
            // KAG callbacks are NEVER invoked here   this is the
            // boundary that prevents story-advancement leaks.
            for (auto& cb : m_gameCallbacks) {
                if (m_focus != InputFocus::GAME) break;
                cb(event);
            }
            break;
        }
    }
}

// -------------------------------------------------------------------------------
//  InputRouter::submitPointer   unified pointer path (Track P3)
// -------------------------------------------------------------------------------
// Source-agnostic: builds the equivalent SDL_Event and feeds the SAME
// dispatch (single source of truth for KAG/GAME routing + click-pending).
// MobileAdapter's SDL touch->mouse injection remains the compat path.

void InputRouter::submitPointer(const PointerEvent& event) {
    m_activePointers = event.activePointers;

    SDL_Event ev{};
    switch (event.action) {
        case PointerAction::Down: {
            ev.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            ev.button.button = SDL_BUTTON_LEFT;
            ev.button.x = static_cast<float>(event.x);
            ev.button.y = static_cast<float>(event.y);
            dispatchSdlEvent(ev);
            break;
        }
        case PointerAction::Move: {
            ev.type = SDL_EVENT_MOUSE_MOTION;
            ev.motion.x = static_cast<float>(event.x);
            ev.motion.y = static_cast<float>(event.y);
            dispatchSdlEvent(ev);
            break;
        }
        case PointerAction::Up: {
            ev.type = SDL_EVENT_MOUSE_BUTTON_UP;
            ev.button.button = SDL_BUTTON_LEFT;
            ev.button.x = static_cast<float>(event.x);
            ev.button.y = static_cast<float>(event.y);
            dispatchSdlEvent(ev);
            break;
        }
        case PointerAction::LongPress: {
            // Right-button press+release pair (MobileAdapter long-press
            // semantics: right-click action on a held contact).
            ev.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            ev.button.button = SDL_BUTTON_RIGHT;
            ev.button.x = static_cast<float>(event.x);
            ev.button.y = static_cast<float>(event.y);
            dispatchSdlEvent(ev);
            ev.button.button = SDL_BUTTON_RIGHT;
            ev.type = SDL_EVENT_MOUSE_BUTTON_UP;
            dispatchSdlEvent(ev);
            break;
        }
        case PointerAction::Pinch: {
            // Wheel delta derived from the cumulative scale (like the
            // MobileAdapter wheel injection); first pinch event = baseline.
            if (m_lastPinchScale <= 0.0f) m_lastPinchScale = event.scale;
            const float delta = event.scale - m_lastPinchScale;
            m_lastPinchScale = event.scale;
            ev.type = SDL_EVENT_MOUSE_WHEEL;
            ev.wheel.y = static_cast<float>(delta) * 10.0f;
            dispatchSdlEvent(ev);
            break;
        }
    }
}

// -------------------------------------------------------------------------------
//  InputRouter::setFocus   atomic focus switch with boundary hardening
// -------------------------------------------------------------------------------
//
//  When switching:
//      GAME: Drains all pending KAG click state.
//            This includes the event currently being dispatched when a
//            KAG callback changes focus. Future events route to GAME.
//
//      KAG:  Resets the click-pending flag to false.
//            The GAME-mode callbacks have had their last event.
//            The first KAG click after the switch will set the flag.
//
//  The kagClickPending drain is the KEY guarantee that no phantom
//  click leaks from a KAG GAME KAG round-trip.
// -------------------------------------------------------------------------------

void InputRouter::setFocus(InputFocus focus) {
    if (!isValidFocus(focus)) {
        printf("[InputRouter] Ignoring invalid focus: %d\n", static_cast<int>(focus));
        return;
    }
    if (m_focus == focus) return;

    InputFocus oldFocus = m_focus;
    m_focus = focus;

    // -- Boundary hardening ----------------------------------------------
    // When switching TO GAME, forcefully drain any pending KAG click state.
    // This guarantees no phantom click leaks through when focus returns
    // to KAG later, including a click that triggered the switch itself.
    //
    // When switching TO KAG, also drain the flag. This ensures a clean
    // slate   the next user click sets the flag, rather than a stale
    // flag from before the GAME session causing an instant advance.
    m_kagClickPending = false;
    m_lastPinchScale = 0.0f;

    printf("[InputRouter] Focus: %s   %s (input=%s, gameCb=%zu, kagCb=%zu)\n",
           inputFocusToString(oldFocus), inputFocusToString(focus),
           (focus == InputFocus::GAME) ? "game-only" : "kag-only",
           m_gameCallbacks.size(), m_kagCallbacks.size());

    // Notify focus change callbacks (e.g. for UI state updates)
    for (auto& cb : m_focusChangeCallbacks) {
        if (m_focus != focus) break;
        cb(focus);
    }
}

void InputRouter::registerGameCallback(GameInputCallback cb) {
    m_gameCallbacks.push_back(std::move(cb));
}

void InputRouter::registerKAGCallback(GameInputCallback cb) {
    m_kagCallbacks.push_back(std::move(cb));
}

void InputRouter::registerFocusChangeCallback(FocusChangeCallback cb) {
    m_focusChangeCallbacks.push_back(std::move(cb));
}

// -------------------------------------------------------------------------------
//  InputRouter::registerResizeCallback / notifyResize
// -------------------------------------------------------------------------------
//
//  Called by Engine::processEvents() when SDL_EVENT_WINDOW_RESIZED fires.
//  Notifies all registered callbacks so that layers.lua can rebuild the
//  layer tree and mark all layers dirty for the new viewport dimensions.
// -------------------------------------------------------------------------------

void InputRouter::registerResizeCallback(ResizeCallback cb) {
    m_resizeCallbacks.push_back(std::move(cb));
}

void InputRouter::notifyResize(int newWidth, int newHeight) {
    for (auto& cb : m_resizeCallbacks) {
        cb(newWidth, newHeight);
    }
}

} // namespace Caesura