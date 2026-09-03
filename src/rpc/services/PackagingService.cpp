// PackagingService.cpp -- build/package logic extracted from EditorServer.cpp.

#include "PackagingService.h"

#include "archive/api/IArchiveWriter.h"

#include <algorithm>
#include <cstdint>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace Caesura {
namespace rpc {
namespace service {

namespace {

bool isStoryPathAllowed(const std::string& path) {
    static const char* const kPrefixes[] = {
        "assets/", "demo/", "tests/projects/", "projects/"};
    for (const char* prefix : kPrefixes) {
        if (path.rfind(prefix, 0) == 0) return true;
    }
    return false;
}

bool sanitizeWebOutName(const std::string& raw, std::string& out) {
    out.clear();
    for (unsigned char ch : raw) {
        // A1: keep every UTF-8 code point (Chinese, CJK, parentheses,
        // percent, spaces all survive); strip only control characters,
        // path separators and Windows-illegal filename characters
        // (<>:"|*? — review NIT-1 hardening). ASCII alnum/underscore/
        // hyphen/dot are unchanged. Empty result (nothing but stripped
        // chars) = 400.
        if (ch < 0x20 || ch == 0x7F || ch == '/' || ch == '\\' ||
            ch == '<' || ch == '>' || ch == ':' || ch == '"' ||
            ch == '|' || ch == '*' || ch == '?') {
            continue;
        }
        out.push_back(static_cast<char>(ch));
    }
    return !out.empty();
}

std::string logTailOf(const std::string& log) {
    constexpr size_t kMaxLines = 30;
    std::vector<std::string> lines;
    std::istringstream stream(log);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    if (lines.size() > kMaxLines) {
        lines.erase(lines.begin(),
                    lines.end() - static_cast<std::ptrdiff_t>(kMaxLines));
    }
    std::string joined;
    for (const auto& entry : lines) {
        joined += entry;
        joined += '\n';
    }
    return joined;
}

std::string confineToBuild(const std::string& p) {
    if (p.empty()) return {};
    std::string norm = p;
    for (auto& ch : norm) {
        if (ch == static_cast<char>(92)) ch = '/';
    }
    if (norm.find("..") != std::string::npos) return {};
    if (norm.find(':') != std::string::npos) return {};
    if (norm[0] == '/') return {};
    if (norm.rfind("build/", 0) != 0) norm = "build/" + norm;
    return norm;
}

#if defined(_WIN32)

bool isRegularFile(const fs::path& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec) && !ec;
}

std::wstring lowerPathForCompare(const fs::path& path) {
    std::wstring value = path.generic_wstring();
    for (wchar_t& ch : value) {
        if (ch == L'\\') ch = L'/';
        if (ch >= L'A' && ch <= L'Z') {
            ch = static_cast<wchar_t>(ch - L'A' + L'a');
        }
    }
    return value;
}

bool isWindowsSystemBash(const fs::path& path) {
    const std::wstring value = lowerPathForCompare(path);
    return value.find(L"/system32/") != std::wstring::npos;
}

fs::path findGitBash() {
    // CAESURA_BASH is an explicit operator override. It is intentionally
    // checked before PATH so a machine with WSL's System32/bash.exe first in
    // PATH cannot silently route package_game.sh through WSL.
    if (const wchar_t* env = _wgetenv(L"CAESURA_BASH")) {
        if (*env) {
            const fs::path overridePath(env);
            if (isRegularFile(overridePath)) return overridePath;
        }
    }

    std::vector<fs::path> candidates;
    const auto addGitRoot = [&candidates](const wchar_t* root) {
        if (!root || !*root) return;
        const fs::path base(root);
        candidates.push_back(base / L"Git" / L"bin" / L"bash.exe");
        candidates.push_back(base / L"Git" / L"usr" / L"bin" / L"bash.exe");
    };
    addGitRoot(_wgetenv(L"ProgramW6432"));
    addGitRoot(_wgetenv(L"ProgramFiles"));
    addGitRoot(_wgetenv(L"ProgramFiles(x86)"));
    if (const wchar_t* localAppData = _wgetenv(L"LOCALAPPDATA")) {
        if (*localAppData) {
            candidates.push_back(fs::path(localAppData) / L"Programs" / L"Git" /
                                 L"bin" / L"bash.exe");
            candidates.push_back(fs::path(localAppData) / L"Programs" / L"Git" /
                                 L"usr" / L"bin" / L"bash.exe");
        }
    }
    for (const auto& candidate : candidates) {
        if (isRegularFile(candidate)) return candidate;
    }

    // Git can be installed outside the standard Program Files locations. Walk
    // PATH ourselves rather than asking CreateProcess/shutil.which to resolve
    // the bare name: Windows puts System32/bash.exe (the WSL launcher) ahead
    // of PATH. Any other explicit PATH candidate is allowed: official
    // PortableGit installs commonly live under names such as PortableGit or
    // GitPortable and do not contain a literal /Git/ path component.
    if (const wchar_t* pathEnv = _wgetenv(L"PATH")) {
        const std::wstring pathList(pathEnv);
        size_t start = 0;
        while (start <= pathList.size()) {
            const size_t end = pathList.find(L';', start);
            std::wstring piece = pathList.substr(
                start, end == std::wstring::npos ? std::wstring::npos : end - start);
            while (piece.size() >= 2 && piece.front() == L'"' && piece.back() == L'"') {
                piece = piece.substr(1, piece.size() - 2);
            }
            if (!piece.empty()) {
                const fs::path candidate = fs::path(piece) / L"bash.exe";
                if (isRegularFile(candidate) && !isWindowsSystemBash(candidate)) {
                    return candidate;
                }
            }
            if (end == std::wstring::npos) break;
            start = end + 1;
        }
    }
    return {};
}

std::wstring quoteWindowsProcessArg(const std::wstring& raw) {
    std::wstring quoted;
    quoted.reserve(raw.size() + 4);
    quoted.push_back(L'"');
    size_t backslashes = 0;
    for (const wchar_t ch : raw) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }
        if (ch == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(ch);
            backslashes = 0;
            continue;
        }
        quoted.append(backslashes, L'\\');
        backslashes = 0;
        quoted.push_back(ch);
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

bool runProcessCapture(const fs::path& executable,
                       const std::vector<std::wstring>& arguments,
                       std::string& output,
                       int& exitCode,
                       DWORD& spawnError) {
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &security, 0)) {
        spawnError = GetLastError();
        return false;
    }

    HANDLE nullInput = CreateFileW(
        L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (nullInput == INVALID_HANDLE_VALUE) {
        spawnError = GetLastError();
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        return false;
    }
    if (!SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0)) {
        spawnError = GetLastError();
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        return false;
    }

    std::wstring commandLine = quoteWindowsProcessArg(executable.native());
    for (const auto& argument : arguments) {
        commandLine.push_back(L' ');
        commandLine += quoteWindowsProcessArg(argument);
    }
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    SIZE_T attributeBytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
    if (attributeBytes == 0) {
        spawnError = GetLastError();
        CloseHandle(nullInput);
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        return false;
    }
    std::vector<unsigned char> attributeStorage(attributeBytes);
    auto* attributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        attributeStorage.data());
    if (!InitializeProcThreadAttributeList(attributeList, 1, 0, &attributeBytes)) {
        spawnError = GetLastError();
        CloseHandle(nullInput);
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        return false;
    }
    HANDLE inheritedHandles[] = {nullInput, writePipe};
    if (!UpdateProcThreadAttribute(
            attributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            inheritedHandles, sizeof(inheritedHandles), nullptr, nullptr)) {
        spawnError = GetLastError();
        DeleteProcThreadAttributeList(attributeList);
        CloseHandle(nullInput);
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        return false;
    }

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = nullInput;
    startup.StartupInfo.hStdOutput = writePipe;
    startup.StartupInfo.hStdError = writePipe;
    startup.lpAttributeList = attributeList;

    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(
        executable.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
        &startup.StartupInfo, &process);
    DeleteProcThreadAttributeList(attributeList);
    CloseHandle(nullInput);
    if (!created) {
        spawnError = GetLastError();
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        return false;
    }
    CloseHandle(writePipe);

    constexpr size_t kMaxLogBytes = 1u << 20;
    char buffer[4096];
    DWORD bytesRead = 0;
    while (ReadFile(readPipe, buffer, sizeof(buffer), &bytesRead, nullptr) &&
           bytesRead > 0) {
        if (output.size() < kMaxLogBytes) {
            const size_t remaining = kMaxLogBytes - output.size();
            output.append(buffer, std::min<size_t>(bytesRead, remaining));
        }
    }
    CloseHandle(readPipe);

    const DWORD waitResult = WaitForSingleObject(process.hProcess, INFINITE);
    if (waitResult != WAIT_OBJECT_0) {
        spawnError = GetLastError();
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return false;
    }
    DWORD processExit = 0;
    if (!GetExitCodeProcess(process.hProcess, &processExit)) {
        spawnError = GetLastError();
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    exitCode = static_cast<int>(processExit);
    return true;
}

#endif

} // namespace

std::wstring PackagingService::widenUtf8(const std::string& utf8) {
#if defined(_WIN32)
    // True UTF-8 -> UTF-16 via MultiByteToWideChar. Strict mode first
    // (MB_ERR_INVALID_CHARS rejects malformed input); on failure fall back
    // to the flag-less conversion, which substitutes U+FFFD per invalid
    // unit -- deterministic and never crashes (the old widenAscii mapped
    // each byte to a Latin-1 wchar, producing U+00E4 U+00B8 U+00AD for 中).
    if (utf8.empty()) return std::wstring();
    const int count = static_cast<int>(utf8.size());
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                     utf8.data(), count, nullptr, 0);
    if (length == 0) {
        length = MultiByteToWideChar(CP_UTF8, 0,
                                     utf8.data(), count, nullptr, 0);
    }
    std::wstring out(static_cast<size_t>(length), L'\0');
    if (length > 0) {
        MultiByteToWideChar(CP_UTF8, 0,
                            utf8.data(), count, out.data(), length);
    }
    return out;
#else
    // POSIX: wchar_t is 32-bit, so one decoded code point maps to one
    // element. Minimal UTF-8 decoder; malformed bytes (bad lead byte,
    // truncated/invalid continuation, overlong, surrogates, > U+10FFFF)
    // yield U+FFFD and advance one byte -- mirrors the Windows fallback.
    std::wstring out;
    out.reserve(utf8.size());
    const size_t n = utf8.size();
    size_t i = 0;
    while (i < n) {
        const unsigned char b = static_cast<unsigned char>(utf8[i]);
        unsigned int cp = 0;
        int extra = 0;
        if (b < 0x80) {
            cp = b;
            out.push_back(static_cast<wchar_t>(cp));
            ++i;
            continue;
        } else if ((b & 0xE0) == 0xC0) {
            cp = b & 0x1F; extra = 1;
        } else if ((b & 0xF0) == 0xE0) {
            cp = b & 0x0F; extra = 2;
        } else if ((b & 0xF8) == 0xF0) {
            cp = b & 0x07; extra = 3;
        } else {
            out.push_back(0xFFFD); ++i; continue;
        }
        if (i + static_cast<size_t>(extra) >= n) {
            // Truncated sequence: continuation bytes are missing.
            out.push_back(0xFFFD); ++i; continue;
        }
        bool ok = true;
        for (int k = 1; k <= extra; ++k) {
            const unsigned char c = static_cast<unsigned char>(utf8[i + static_cast<size_t>(k)]);
            if ((c & 0xC0) != 0x80) { ok = false; break; }
            cp = (cp << 6) | (c & 0x3F);
        }
        const unsigned int minCp = extra == 1 ? 0x80u : (extra == 2 ? 0x800u : 0x10000u);
        if (!ok || cp < minCp || cp > 0x10FFFF ||
            (cp >= 0xD800 && cp <= 0xDFFF)) {
            // Invalid continuation / overlong / surrogate / out of range.
            out.push_back(0xFFFD); ++i; continue;
        }
        out.push_back(static_cast<wchar_t>(cp));
        i += static_cast<size_t>(extra) + 1;
    }
    return out;
#endif
}

ServiceResult PackagingService::build(const std::string& rawOutputPath,
                                      const std::string& rawKeyPath) {
    std::string outputPath = confineToBuild(rawOutputPath);
    std::string keyPath = confineToBuild(rawKeyPath);
    if (outputPath.empty() || keyPath.empty()) {
        return ServiceResult::err(400, "outputPath/keyPath must be relative paths under build/");
    }

    std::vector<std::pair<std::string, std::string>> files;
    for (const char* dir : {"scripts", "assets"}) {
        if (!fs::exists(dir)) continue;
        try {
            for (const auto& entry : fs::recursive_directory_iterator(dir)) {
                if (!entry.is_regular_file()) continue;
                std::string rel = entry.path().string();
                for (auto& c : rel) if (c == '\\') c = '/';
                files.push_back({rel, entry.path().string()});
            }
        } catch (...) {}
    }
    if (files.empty()) return ServiceResult::err(400, "No files to package");

    std::error_code ec;
    fs::create_directories("build", ec);

    auto writer = m_writerFactory ? m_writerFactory() : nullptr;
    if (!writer) return ServiceResult::err(503, "Archive writer is not configured");
    if (!writer->create(outputPath, keyPath, keyPath + ".pub")) {
        return ServiceResult::err(500, "Failed to create CARC archive");
    }
    for (const auto& [relPath, diskPath] : files) {
        std::ifstream ifs(diskPath, std::ios::binary);
        if (!ifs.is_open()) continue;
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)),
                                  std::istreambuf_iterator<char>());
        writer->addFile(relPath, data.data(), data.size());
    }
    if (!writer->finalize()) return ServiceResult::err(500, "Failed to finalize CARC archive");

    std::error_code se;
    const uintmax_t fileSize = fs::file_size(outputPath, se);
    return ServiceResult::ok({{"status", "ok"},
                              {"path", outputPath},
                              {"size", static_cast<unsigned long long>(fileSize)},
                              {"files", files.size()}});
}

ServiceResult PackagingService::packageWeb(const std::string& storyPath,
                                           const std::string& outName) {
    std::string sp = storyPath;
    for (auto& ch : sp) {
        if (ch == static_cast<char>(92)) ch = '/';
    }
    if (sp.find("..") != std::string::npos) {
        return ServiceResult::err(400, "storyPath must not contain '..'");
    }
    if (sp.find(':') != std::string::npos) {
        return ServiceResult::err(400, "storyPath must be a relative repository path (no drive/scheme)");
    }
    if (!sp.empty() && sp.front() == '/') {
        return ServiceResult::err(400, "storyPath must be a relative repository path");
    }
    for (unsigned char ch : sp) {
        // A1: Unicode story names are allowed (Chinese, parentheses,
        // percent, spaces all pass); only control characters remain
        // forbidden -- a control byte in a path is never legitimate.
        if (ch < 0x20 || ch == 0x7F) {
            return ServiceResult::err(400,
                "storyPath contains control characters (Unicode paths are allowed)");
        }
    }
    if (!isStoryPathAllowed(sp)) {
        return ServiceResult::err(400, "storyPath must live under assets/, demo/, tests/projects/ or projects/");
    }

    std::string name = outName.empty() ? "example_game" : outName;
    if (!outName.empty() && !sanitizeWebOutName(outName, name)) {
        return ServiceResult::err(400, "outName contains no usable characters ([A-Za-z0-9_-])");
    }
    const std::string outDir = "dist/" + name;

    fs::path scriptPath = m_ctx.sourceRoot() / "scripts" / "package_game.sh";
    if (!fs::exists(scriptPath)) {
        return ServiceResult{503, {{"ok", false},
                                   {"error", "scripts/package_game.sh not found (engine must run inside the repository)"}}};
    }

    try {
        const fs::path repoRoot = scriptPath.parent_path().parent_path();
        // A1: fs::path(std::string) converts the narrow string via the
        // ANSI code page on MSVC (UTF-8 中文 -> GBK garbage -> "not found").
        // u8path interprets the bytes as UTF-8, matching the JSON contract.
        if (!fs::exists(repoRoot / fs::u8path(sp))) {
            return ServiceResult::err(400, "storyPath not found: " + sp);
        }
    } catch (...) {
        return ServiceResult{503, {{"ok", false},
                                   {"error", "failed to resolve storyPath against the engine source root"}}};
    }
#if !defined(_WIN32)
    std::string scriptArg;
    try {
        scriptArg = fs::relative(scriptPath, m_ctx.buildRoot()).generic_string();
    } catch (...) {
        scriptArg.clear();
    }
    if (scriptArg.empty()) {
        return ServiceResult{503, {{"ok", false},
                                   {"error", "failed to resolve scripts/package_game.sh relative to the engine working directory"}}};
    }
    for (auto& ch : scriptArg) {
        if (ch == static_cast<char>(92)) ch = '/';
    }
#endif
    std::string output;
    int exitCode = -1;
#if defined(_WIN32)
    const fs::path bashPath = findGitBash();
    if (bashPath.empty()) {
        return ServiceResult{503, {{"ok", false},
                                   {"error", "Git Bash not found (set CAESURA_BASH to bash.exe)"},
                                   {"outputDir", outDir}}};
    }
    DWORD spawnError = ERROR_SUCCESS;
    if (!runProcessCapture(
            bashPath,
            {scriptPath.native(), L"--out", widenUtf8(outDir), widenUtf8(sp)},
            output, exitCode, spawnError)) {
        return ServiceResult{500, {{"ok", false},
                                   {"error", "failed to spawn Git Bash (Windows error " +
                                                 std::to_string(spawnError) + ")"},
                                   {"outputDir", outDir}}};
    }
#else
    const std::string command = "bash \"" + scriptArg + "\" --out \"" +
                                outDir + "\" \"" + sp + "\" 2>&1";
    FILE* pipe = ::popen(command.c_str(), "r");
    if (!pipe) {
        const char* const spawnError = "failed to spawn bash (is git bash on PATH?)";
        return ServiceResult{500, {{"ok", false},
                                   {"error", spawnError},
                                   {"outputDir", outDir}}};
    }
    constexpr size_t kMaxLogBytes = 1u << 20;
    char buffer[4096];
    size_t chunk = 0;
    while ((chunk = fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
        if (output.size() < kMaxLogBytes) output.append(buffer, chunk);
    }
    exitCode = ::pclose(pipe);
#endif
    const std::string tail = logTailOf(output);

    if (exitCode != 0) {
        return ServiceResult{500, {{"status", "error"},
                                   {"ok", false},
                                   {"error", "package_game.sh failed with exit code " +
                                                 std::to_string(exitCode)},
                                   {"outputDir", outDir},
                                   {"logTail", tail}}};
    }
    return ServiceResult::ok({{"status", "ok"},
                              {"ok", true},
                              {"outputDir", outDir},
                              {"logTail", tail}});
}

} // namespace service
} // namespace rpc
} // namespace Caesura
