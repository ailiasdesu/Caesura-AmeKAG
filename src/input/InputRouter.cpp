#include "InputRouter.h"
#include <cstdio>

namespace Caesura {

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
    switch (m_focus) {
        case InputFocus::KAG: {
            // In KAG mode, mouse clicks and keyboard advance the story
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                event.type == SDL_EVENT_KEY_DOWN) {

                // Forward to KAG callbacks (typically resumes Lua coroutine)
                for (auto& cb : m_kagCallbacks) {
                    cb(event);
                }

                // Mark a click as pending for wait_click() consumption
                m_kagClickPending = true;
            }
            break;
        }
        case InputFocus::GAME: {
            // In GAME mode, dispatch ALL events to game callbacks.
            // KAG callbacks are NEVER invoked here   this is the
            // boundary that prevents story-advancement leaks.
            for (auto& cb : m_gameCallbacks) {
                cb(event);
            }
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
//            Any in-flight KAG coroutine resume from a click that
//            happened BEFORE this call has already been processed
//            (SDL events are synchronous). Future clicks route to GAME.
//
//      KAG:  Resets the click-pending flag to false.
//            The GAME-mode callbacks have had their last event.
//            The first KAG click after the switch will set the flag.
//
//  The kagClickPending drain is the KEY guarantee that no phantom
//  click leaks from a KAG GAME KAG round-trip.
// -------------------------------------------------------------------------------

void InputRouter::setFocus(InputFocus focus) {
    if (m_focus == focus) return;

    InputFocus oldFocus = m_focus;
    m_focus = focus;

    // -- Boundary hardening ----------------------------------------------
    // When switching TO GAME, forcefully drain any pending KAG click state.
    // This guarantees no phantom click leaks through when focus returns
    // to KAG later. The click that triggered the switch (if any) was
    // already consumed by the KAG callback before setFocus was called.
    //
    // When switching TO KAG, also drain the flag. This ensures a clean
    // slate   the next user click sets the flag, rather than a stale
    // flag from before the GAME session causing an instant advance.
    m_kagClickPending = false;

    printf("[InputRouter] Focus: %s   %s (input=%s, gameCb=%zu, kagCb=%zu)\n",
           inputFocusToString(oldFocus), inputFocusToString(focus),
           (focus == InputFocus::GAME) ? "game-only" : "kag-only",
           m_gameCallbacks.size(), m_kagCallbacks.size());

    // Notify focus change callbacks (e.g. for UI state updates)
    for (auto& cb : m_focusChangeCallbacks) {
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
