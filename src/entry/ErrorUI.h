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

// -- DiagnosticInfo ---------------------------------------------------------
// Player-friendly crash context (product spec §15 Crash/Diagnostics).
// Filled in by the composition root (Engine::handleFatalError); default values
// make DiagnosticInfo{} usable anywhere (backward compatible).
struct DiagnosticInfo {
    std::string projectName = "Caesura (AmeKAG)";
    std::string engineVersion;   // CAESURA_VERSION compile definition
    std::string platform;        // SDL_GetPlatform()
    std::string gpuBackend = "unknown";  // renderDevice ? getBackendName() : "unknown"
    std::string scenePath;       // currently running KAG scene (empty = none)
    std::string logDir = "logs"; // log directory (relative ok; resolved on open)
};

// -- ErrorUI ---------------------------------------------------------------
// Two-level error display:
//   Level 1 (bgfx alive): hardware-rendered deep-red screen + white text
//                          using bgfx debug text + view clear.
//   Level 2 (bgfx dead):   SDL_ShowSimpleMessageBox fallback.
//
// Interaction: [R]etry  [T]itle  [Q]uit (or Escape)
//              [C]opy diagnostic text  [O]pen log folder
//
// Phase 8.3: Enhanced error recovery logic.
//   - Retry whitelist: only text-based ops are retry-safe
//   - Animation/transition ops -> retry auto-promotes to title
//   - Same token 3 consecutive retries -> auto title
//   - 3 consecutive title failures -> SDL_ShowSimpleMessageBox + quit
//
// §15: DiagnosticInfo block rendered on both levels; [C] copies the full
// diagnostic text to the clipboard, [O] opens the log folder.

class ErrorUI {
public:
    // Show error screen and block until user selects an action.
    // bgfxAlive: if false, skip directly to Level 2.
    // commandName: the KAG command that triggered the error (for whitelist check)
    // tokenLine: line number in the script
    // diag: optional crash context (default empty = backward compatible)
    static ErrorAction show(
        const std::string& title,
        const std::string& message,
        const std::string& scriptTrace = "",
        int tokenLine = 0,
        bool bgfxAlive = true,
        const std::string& commandName = "",
        const DiagnosticInfo& diag = {}
    );

    // Pure formatter (unit-testable, no SDL/bgfx dependency).
    // Renders the diagnostic block:
    //   <title>
    //   Project: <projectName>
    //   Version: <engineVersion>
    //   Platform: <platform>
    //   GPU: <gpuBackend>
    //   Scene: <scenePath>        (omitted when empty)
    //   Script: <scriptTrace>     (omitted when empty)
    //   Line: <tokenLine>         (omitted when <= 0)
    //   Command: <commandName>    (omitted when empty)
    //
    //   <message>
    static std::string buildDiagnosticText(const DiagnosticInfo& diag,
                                           const std::string& title,
                                           const std::string& message,
                                           const std::string& scriptTrace,
                                           int tokenLine,
                                           const std::string& commandName);

    // Level 2: SDL MessageBox (always works, no GPU needed).
    // diag: optional crash context appended to the message body.
    static ErrorAction showFallback(
        const std::string& title,
        const std::string& message,
        const DiagnosticInfo& diag = {}
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
        int screenW, int screenH,
        const std::string& diagBlock,
        const std::string& statusText
    );
    static ErrorAction waitForInput();

    // Crash tracking state
    static int m_retryCount;
    static int m_titleCrashCount;
    static std::string m_lastTokenLocation;

    // Retry whitelist -- commands safe to retry
    static const std::unordered_set<std::string> s_retryWhitelist;

    // Animation/transition ops -- auto-promote retry to title
    static const std::unordered_set<std::string> s_animateOps;
};

} // namespace Caesura
