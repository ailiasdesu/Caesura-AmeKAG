#pragma once
#include <string>
#include <functional>
#include <unordered_set>

namespace Caesura {

enum class ErrorAction {
    Retry,
    Title,
    Quit
};

// -- ErrorUI ---------------------------------------------------------------
// Two-level error display:
//   Level 1 (bgfx alive): hardware-rendered deep-red screen + white text
//                          using bgfx debug text + view clear.
//   Level 2 (bgfx dead):   SDL_ShowSimpleMessageBox fallback.
//
// Interaction: [R]etry  [T]itle  [Q]uit (or Escape)
//
// Phase 8.3: Enhanced error recovery logic.
//   - Retry whitelist: only text-based ops are retry-safe
//   - Animation/transition ops → retry auto-promotes to title
//   - Same token 3 consecutive retries → auto title
//   - 3 consecutive title failures → SDL_ShowSimpleMessageBox + quit

class ErrorUI {
public:
    // Show error screen and block until user selects an action.
    // bgfxMustBeAlive: if false, skip directly to Level 2.
    // commandName: the KAG command that triggered the error (for whitelist check)
    // tokenLine: line number in the script
    static ErrorAction show(
        const std::string& title,
        const std::string& message,
        const std::string& scriptTrace = "",
        int tokenLine = 0,
        bool bgfxAlive = true,
        const std::string& commandName = ""
    );

    // Level 2: SDL MessageBox (always works, no GPU needed)
    static ErrorAction showFallback(
        const std::string& title,
        const std::string& message
    );

    // Reset crash counters (e.g., after successful title return)
    static void resetCounters();

    // Check if a command is retry-safe (whitelist)
    static bool isRetrySafe(const std::string& commandName);

private:
    static void renderLevel1(
        const std::string& title,
        const std::string& message,
        const std::string& scriptTrace,
        int tokenLine,
        int screenW, int screenH
    );
    static ErrorAction waitForInput();

    // Crash tracking state
    static int m_retryCount;
    static int m_titleCrashCount;
    static std::string m_lastTokenLocation;

    // Retry whitelist — commands safe to retry
    static const std::unordered_set<std::string> s_retryWhitelist;

    // Animation/transition ops — auto-promote retry to title
    static const std::unordered_set<std::string> s_animateOps;
};

} // namespace Caesura