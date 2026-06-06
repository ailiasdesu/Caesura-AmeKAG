#include "ErrorUI.h"
#include <bgfx/bgfx.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>

namespace Caesura {

// Static state initialization
int         ErrorUI::m_retryCount        = 0;
int         ErrorUI::m_titleCrashCount    = 0;
std::string ErrorUI::m_lastTokenLocation;

// Retry whitelist — commands where a retry is safe
const std::unordered_set<std::string> ErrorUI::s_retryWhitelist = {
    "text", "ch", "bg", "fg", "cl", "image",
    "playbgm", "stopbgm", "playse", "stopse", "setvolume",
    "wait", "if", "else", "endif", "ruby", "font", "current",
};

// Animation/transition ops — retry auto-promotes to title
const std::unordered_set<std::string> ErrorUI::s_animateOps = {
    "move", "trans", "quake", "fade",
    "anim", "transition", "quakex", "quakey",
    "wm", "zoom", "rotate",
};

bool ErrorUI::isRetrySafe(const std::string& commandName) {
    if (commandName.empty()) return false;
    // Animation/transition ops are NOT retry-safe → promote to title
    if (s_animateOps.count(commandName)) return false;
    return s_retryWhitelist.count(commandName) > 0;
}

void ErrorUI::resetCounters() {
    m_retryCount = 0;
    m_titleCrashCount = 0;
    m_lastTokenLocation.clear();
}

ErrorAction ErrorUI::show(
    const std::string& title,
    const std::string& message,
    const std::string& scriptTrace,
    int tokenLine,
    bool bgfxAlive,
    const std::string& commandName)
{
    // Build token location key for same-token tracking
    std::string tokenLoc = scriptTrace + ":" + std::to_string(tokenLine);

    // Auto-promotion logic (processed before showing UI)
    ErrorAction forcedAction = ErrorAction::Retry;  // sentinel: no force

    // Check: animation/transition ops — retry auto-promotes to title
    if (!commandName.empty() && !isRetrySafe(commandName)) {
        if (s_animateOps.count(commandName)) {
            // This command's retry would auto-promote to title.
            // We don't force yet — let user choose, but show a note.
        }
    }

    // Check: title crash counter
    if (m_titleCrashCount >= 3) {
        // Fatal: 3 consecutive titles failed → hard quit
        fprintf(stderr, "\n=== CAESURA FATAL: 3 consecutive title crashes ===\n");
        fprintf(stderr, "Token: %s:%d | Command: %s\n",
                scriptTrace.c_str(), tokenLine, commandName.c_str());
        fprintf(stderr, "The engine cannot recover. Exiting.\n");
        showFallback(
            "FATAL ERROR — Engine Cannot Recover",
            "3 consecutive attempts to return to title have failed.\n"
            "The engine will now exit.\n\n"
            "Last error: " + title + "\n" +
            message
        );
        return ErrorAction::Quit;
    }

    // Check: same token 3 consecutive retries → auto title
    bool sameToken = (!tokenLoc.empty() && tokenLoc == m_lastTokenLocation);
    if (!sameToken) {
        m_retryCount = 0;
        m_lastTokenLocation = tokenLoc;
    }

    // --- Display the error screen ---
    if (!bgfxAlive || !SDL_WasInit(SDL_INIT_VIDEO)) {
        m_titleCrashCount++;
        return showFallback(title, message);
    }

    const bgfx::Stats* stats = bgfx::getStats();
    int screenW = 1280, screenH = 720;
    if (stats) {
        screenW = stats->width  > 0 ? stats->width  : screenW;
        screenH = stats->height > 0 ? stats->height : screenH;
    }

    ErrorAction action = ErrorAction::Quit;
    bool running = true;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                action = ErrorAction::Quit;
                running = false;
                break;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.key) {
                    case SDLK_R:
                        // Check whitelist: if animate op, promote to title
                        if (!commandName.empty() && !isRetrySafe(commandName)) {
                            action = ErrorAction::Title;
                        } else {
                            action = ErrorAction::Retry;
                        }
                        running = false;
                        break;
                    case SDLK_T:
                        action = ErrorAction::Title;
                        running = false;
                        break;
                    case SDLK_Q: case SDLK_ESCAPE:
                        action = ErrorAction::Quit;
                        running = false;
                        break;
                }
            }
        }
        if (!running) break;

        bgfx::touch(0);
        renderLevel1(title, message, scriptTrace, tokenLine, screenW, screenH);
        bgfx::frame();
    }

    // Update crash counters based on chosen action
    if (action == ErrorAction::Retry) {
        m_retryCount++;
        if (sameToken && m_retryCount >= 3) {
            // Auto-promote to title
            fprintf(stderr, "[ErrorUI] Same token retried 3 times — auto-promoting to Title.\n");
            m_retryCount = 0;
            m_titleCrashCount++;
            return ErrorAction::Title;
        }
        m_titleCrashCount = 0;  // retry resets title counter
    } else if (action == ErrorAction::Title) {
        m_titleCrashCount++;
        m_retryCount = 0;
    } else {
        // Quit resets all
        m_retryCount = 0;
        m_titleCrashCount = 0;
    }

    return action;
}

void ErrorUI::renderLevel1(
    const std::string& title,
    const std::string& message,
    const std::string& scriptTrace,
    int tokenLine,
    int screenW, int screenH)
{
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0x8b0000ff, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, (uint16_t)screenW, (uint16_t)screenH);

    bgfx::dbgTextClear();
    uint8_t white = 0x0F;
    uint8_t yellow = 0x0E;
    uint8_t green = 0x0A;

    bgfx::dbgTextPrintf(0, 0, 0x4F, "========================================");
    bgfx::dbgTextPrintf(0, 1, 0x4F, "  CAESURA ENGINE -- FATAL ERROR");
    bgfx::dbgTextPrintf(0, 2, 0x4F, "========================================");

    int row = 4;
    bgfx::dbgTextPrintf(0, row++, white, "  %s", title.c_str());

    row++;
    std::string msg = title;
    // Print the actual message
    row--;  // back up to use message
    {
        std::string mm = message;
        size_t pos = 0;
        while (pos < mm.size()) {
            size_t nl = mm.find('\n', pos);
            std::string line = (nl != std::string::npos) ? mm.substr(pos, nl - pos) : mm.substr(pos);
            bgfx::dbgTextPrintf(0, row++, white, "  %s", line.c_str());
            if (nl == std::string::npos) break;
            pos = nl + 1;
        }
    }

    if (!scriptTrace.empty()) {
        row++;
        bgfx::dbgTextPrintf(0, row++, yellow, "  Script trace:");
        bgfx::dbgTextPrintf(0, row++, yellow, "    %s", scriptTrace.c_str());
        if (tokenLine > 0) {
            bgfx::dbgTextPrintf(0, row++, yellow, "    Token line: %d", tokenLine);
        }
    }

    // Crash counter info
    if (m_retryCount > 0) {
        row++;
        bgfx::dbgTextPrintf(0, row++, 0x0C,
                            "  Retry count: %d/3 (same token)", m_retryCount);
    }
    if (m_titleCrashCount > 0) {
        bgfx::dbgTextPrintf(0, row++, 0x0C,
                            "  Title crash count: %d/3", m_titleCrashCount);
    }

    row += 2;
    bgfx::dbgTextPrintf(0, row++, green, "  [R] Retry  -- Reload and retry the current script");
    bgfx::dbgTextPrintf(0, row++, green, "  [T] Title  -- Return to title screen");
    bgfx::dbgTextPrintf(0, row++, green, "  [Q] Quit   -- Exit the engine (Esc)");
}

ErrorAction ErrorUI::showFallback(
    const std::string& title,
    const std::string& message)
{
    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            title.c_str(),
            message.c_str(),
            nullptr
        );
    } else {
        fprintf(stderr, "\n=== FATAL ERROR ===\n%s\n%s\n==================\n",
                title.c_str(), message.c_str());
    }
    return ErrorAction::Quit;
}

} // namespace Caesura