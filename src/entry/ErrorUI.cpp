#include "ErrorUI.h"
#include <bgfx/bgfx.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>

namespace Caesura {

namespace {

namespace fs = std::filesystem;

// Static state initialization lives below (class members).

// Append a "Label: value\n" line to the diagnostic block.
void appendField(std::string& out, const char* label, const std::string& value) {
    out += label;
    out += ": ";
    out += value;
    out += "\n";
}

// Diagnostic field lines only (no title/message) -- shared by Level 1/Level 2
// rendering and buildDiagnosticText. Each line ends with '\n'; absence rules:
// Scene/Script omitted when empty, Line omitted when <= 0, Command omitted
// when empty (product spec §15).
std::string buildDiagBlock(const DiagnosticInfo& diag,
                           const std::string& scriptTrace,
                           int tokenLine,
                           const std::string& commandName) {
    std::string out;
    appendField(out, "Project", diag.projectName);
    appendField(out, "Version", diag.engineVersion);
    appendField(out, "Platform", diag.platform);
    appendField(out, "GPU", diag.gpuBackend.empty() ? std::string("unknown") : diag.gpuBackend);
    if (!diag.scenePath.empty()) appendField(out, "Scene", diag.scenePath);
    if (!scriptTrace.empty())    appendField(out, "Script", scriptTrace);
    if (tokenLine > 0)           appendField(out, "Line", std::to_string(tokenLine));
    if (!commandName.empty())    appendField(out, "Command", commandName);
    return out;
}

// Build a file:// URL for the log folder (§15 [O]). The path is made
// absolute first (fs::absolute); every byte outside RFC 3986 unreserved
// characters plus '/' is percent-encoded (spaces, parens, non-ASCII...).
std::string logDirUrl(const std::string& logDir) {
    const std::string dir = logDir.empty() ? std::string("logs") : logDir;
    std::error_code ec;
    fs::path abs = fs::absolute(fs::path(dir), ec);
    std::string native = ec ? dir : abs.generic_string();
    static const char* kHex = "0123456789ABCDEF";
    auto unreserved = [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '-' || c == '_' ||
               c == '.' || c == '~' || c == '/';
    };
    std::string url = "file:///";
    for (unsigned char c : native) {
        if (unreserved(c)) {
            url += static_cast<char>(c);
        } else {
            url += '%';
            url += kHex[c >> 4];
            url += kHex[c & 0x0F];
        }
    }
    return url;
}

} // namespace

// Static state initialization
int         ErrorUI::m_retryCount     = 0;
int         ErrorUI::m_titleCrashCount = 0;
std::string ErrorUI::m_lastTokenLocation;

// Retry whitelist -- commands where a retry is safe
const std::unordered_set<std::string> ErrorUI::s_retryWhitelist = {
    "text", "ch", "bg", "fg", "cl", "image",
    "playbgm", "stopbgm", "playse", "stopse", "setvolume",
    "wait", "if", "else", "endif", "ruby", "font", "current",
};

// Animation/transition ops -- retry auto-promotes to title
const std::unordered_set<std::string> ErrorUI::s_animateOps = {
    "move", "trans", "quake", "fade",
    "anim", "transition", "quakex", "quakey",
    "wm", "zoom", "rotate",
};

bool ErrorUI::isRetrySafe(const std::string& commandName) {
    if (commandName.empty()) return false;
    // Animation/transition ops are NOT retry-safe -> promote to title
    if (s_animateOps.count(commandName)) return false;
    return s_retryWhitelist.count(commandName) > 0;
}

void ErrorUI::resetCounters() {
    m_retryCount = 0;
    m_titleCrashCount = 0;
    m_lastTokenLocation.clear();
}

std::string ErrorUI::buildDiagnosticText(const DiagnosticInfo& diag,
                                         const std::string& title,
                                         const std::string& message,
                                         const std::string& scriptTrace,
                                         int tokenLine,
                                         const std::string& commandName)
{
    std::string text = title;
    text += "\n";
    text += buildDiagBlock(diag, scriptTrace, tokenLine, commandName);
    text += "\n";
    text += message;
    return text;
}

ErrorAction ErrorUI::show(
    const std::string& title,
    const std::string& message,
    const std::string& scriptTrace,
    int tokenLine,
    bool bgfxAlive,
    const std::string& commandName,
    const DiagnosticInfo& diag)
{
    // Build token location key for same-token tracking
    std::string tokenLoc = scriptTrace + ":" + std::to_string(tokenLine);

    // Precompute diagnostic text once ([C] clipboard + rendering share it).
    const std::string diagText =
        buildDiagnosticText(diag, title, message, scriptTrace, tokenLine, commandName);
    const std::string diagBlock =
        buildDiagBlock(diag, scriptTrace, tokenLine, commandName);

    // Auto-promotion logic (processed before showing UI)
    //
    // Check: title crash counter
    if (m_titleCrashCount >= 3) {
        // Fatal: 3 consecutive titles failed -> hard quit
        fprintf(stderr, "\n=== CAESURA FATAL: 3 consecutive title crashes ===\n");
        fprintf(stderr, "Token: %s:%d | Command: %s\n",
                scriptTrace.c_str(), tokenLine, commandName.c_str());
        fprintf(stderr, "The engine cannot recover. Exiting.\n");
        showFallback(
            "FATAL ERROR -- Engine Cannot Recover",
            "3 consecutive attempts to return to title have failed.\n"
            "The engine will now exit.\n\n"
            "Last error: " + title + "\n" +
            message,
            diag
        );
        return ErrorAction::Quit;
    }

    // Check: same token 3 consecutive retries -> auto title
    bool sameToken = (!tokenLoc.empty() && tokenLoc == m_lastTokenLocation);
    if (!sameToken) {
        m_retryCount = 0;
        m_lastTokenLocation = tokenLoc;
    }

    // --- Display the error screen ---
    if (!bgfxAlive || !SDL_WasInit(SDL_INIT_VIDEO)) {
        m_titleCrashCount++;
        return showFallback(title, message, diag);
    }

    const bgfx::Stats* stats = bgfx::getStats();
    int screenW = 1280, screenH = 720;
    if (stats) {
        screenW = stats->width  > 0 ? stats->width  : screenW;
        screenH = stats->height > 0 ? stats->height : screenH;
    }

    ErrorAction action = ErrorAction::Quit;
    bool running = true;
    std::string statusText;  // transient [C]/[O] feedback line

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
                    case SDLK_C:
                        // [C]: copy the full diagnostic text to the clipboard
                        if (SDL_SetClipboardText(diagText.c_str())) {
                            statusText = "[Diagnostics copied to clipboard]";
                        } else {
                            statusText = "[Copy failed: clipboard unavailable]";
                        }
                        break;
                    case SDLK_O: {
                        // [O]: open the log folder (absolute path via fs::absolute);
                        // on failure degrade to an on-screen notice.
                        const std::string url = logDirUrl(diag.logDir);
                        if (SDL_OpenURL(url.c_str())) {
                            statusText = "[Opened log folder]";
                        } else {
                            statusText = "[Could not open log folder] " + url;
                        }
                        break;
                    }
                }
            }
        }
        if (!running) break;

        bgfx::touch(0);
        renderLevel1(title, message, scriptTrace, tokenLine,
                     screenW, screenH, diagBlock, statusText);
        bgfx::frame();
    }

    // Update crash counters based on chosen action
    if (action == ErrorAction::Retry) {
        m_retryCount++;
        if (sameToken && m_retryCount >= 3) {
            // Auto-promote to title
            fprintf(stderr, "[ErrorUI] Same token retried 3 times -- auto-promoting to Title.\n");
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
    int screenW, int screenH,
    const std::string& diagBlock,
    const std::string& statusText)
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

    // -- Diagnostic block (§15) -------------------------------------------
    if (!diagBlock.empty()) {
        row++;
        size_t pos = 0;
        while (pos < diagBlock.size()) {
            size_t nl = diagBlock.find('\n', pos);
            std::string line = (nl != std::string::npos)
                ? diagBlock.substr(pos, nl - pos) : diagBlock.substr(pos);
            bgfx::dbgTextPrintf(0, row++, yellow, "  %s", line.c_str());
            if (nl == std::string::npos) break;
            pos = nl + 1;
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
    bgfx::dbgTextPrintf(0, row++, green, "  [C] Copy diagnostic text to clipboard");
    bgfx::dbgTextPrintf(0, row++, green, "  [O] Open log folder");

    if (!statusText.empty()) {
        bgfx::dbgTextPrintf(0, row + 1, green, "  %s", statusText.c_str());
    }
}

ErrorAction ErrorUI::showFallback(
    const std::string& title,
    const std::string& message,
    const DiagnosticInfo& diag)
{
    // §15: keep the diagnostic context visible even when the GPU is gone.
    std::string body = message;
    body += "\n\n---- Diagnostic Info ----\n";
    body += buildDiagBlock(diag, "", 0, "");

    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            title.c_str(),
            body.c_str(),
            nullptr
        );
    } else {
        fprintf(stderr, "\n=== FATAL ERROR ===\n%s\n%s\n==================\n",
                title.c_str(), body.c_str());
    }
    return ErrorAction::Quit;
}

} // namespace Caesura
