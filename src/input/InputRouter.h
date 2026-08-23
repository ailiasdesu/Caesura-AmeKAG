#pragma once
#include "api/IInputRouter.h"
#include <SDL3/SDL.h>
#include <functional>
#include <string>
#include <vector>

namespace Caesura {

// ============================================================================
// InputRouter -- implements IInputRouter
// ============================================================================

class InputRouter : public IInputRouter {
public:
    InputRouter() = default;

    void processEvent(const SDL_Event& event) override;
    void submitPointer(const PointerEvent& event) override;
    void setFocus(InputFocus focus) override;
    InputFocus getFocus() const override { return m_focus; }

    void registerGameCallback(GameInputCallback cb) override;
    void registerKAGCallback(GameInputCallback cb) override;
    void registerFocusChangeCallback(FocusChangeCallback cb) override;
    void registerResizeCallback(ResizeCallback cb) override;
    void notifyResize(int newWidth, int newHeight) override;

    bool hasKAGClick() const override { return m_kagClickPending; }
    size_t getKAGCallbackCount() const override  { return m_kagCallbacks.size(); }
    size_t getGameCallbackCount() const override { return m_gameCallbacks.size(); }
    bool isClickPending() const override { return m_kagClickPending; }
    void consumeKAGClick() override { m_kagClickPending = false; }

private:
    void dispatchSdlEvent(const SDL_Event& event);

    InputFocus m_focus = InputFocus::KAG;
    bool m_kagClickPending = false;
    float m_lastPinchScale = 0.0f;   // 0 = no active pinch (P3)
    size_t m_activePointers = 0;     // multi-touch bookkeeping (P3)

    std::vector<GameInputCallback> m_gameCallbacks;
    std::vector<GameInputCallback> m_kagCallbacks;
    std::vector<FocusChangeCallback> m_focusChangeCallbacks;
    std::vector<ResizeCallback> m_resizeCallbacks;
};

const char* inputFocusToString(InputFocus focus);

} // namespace Caesura