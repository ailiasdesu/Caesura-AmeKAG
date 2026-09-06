#pragma once
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <SDL3/SDL.h>
#include <string>
namespace CaesuraTest {
inline bool isGpuChildProcess(const wchar_t* envName) {
    wchar_t value[2] = {};
    return GetEnvironmentVariableW(envName, value, 2) > 0;
}

inline DWORD runGpuChildProcess(const wchar_t* envName, const wchar_t* testCaseName) {
    wchar_t executable[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return ERROR_INSUFFICIENT_BUFFER;

    const DWORD previousSize = GetEnvironmentVariableW(envName, nullptr, 0);
    std::wstring previousValue;
    if (previousSize > 0) {
        previousValue.resize(previousSize);
        GetEnvironmentVariableW(
            envName, previousValue.data(), previousSize);
        previousValue.resize(previousSize - 1);
    }

    if (!SetEnvironmentVariableW(envName, L"1")) return GetLastError();

    std::wstring command =
        L"\"" + std::wstring(executable) + L"\" --test-case=\"" +
        testCaseName + L"\" --no-version";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(
        nullptr,
        command.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startup,
        &process);
    const DWORD createError = created ? ERROR_SUCCESS : GetLastError();

    SetEnvironmentVariableW(
        envName,
        previousSize > 0 ? previousValue.c_str() : nullptr);
    if (!created) return createError;

    const DWORD waitResult = WaitForSingleObject(process.hProcess, 60000);
    DWORD exitCode = ERROR_TIMEOUT;
    if (waitResult == WAIT_OBJECT_0) {
        GetExitCodeProcess(process.hProcess, &exitCode);
    } else {
        TerminateProcess(process.hProcess, ERROR_TIMEOUT);
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exitCode;
}

class HiddenSdlWindow {
public:
    HiddenSdlWindow(int width, int height) {
        if (!SDL_Init(SDL_INIT_VIDEO)) return;
        m_sdlInitialized = true;

        const SDL_PropertiesID props = SDL_CreateProperties();
        if (props == 0) return;
        SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING,
                              "Caesura VFX GPU smoke");
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
        m_window = SDL_CreateWindowWithProperties(props);
        SDL_DestroyProperties(props);
    }

    ~HiddenSdlWindow() {
        if (m_window) SDL_DestroyWindow(m_window);
        if (m_sdlInitialized) SDL_Quit();
    }

    HiddenSdlWindow(const HiddenSdlWindow&) = delete;
    HiddenSdlWindow& operator=(const HiddenSdlWindow&) = delete;

    explicit operator bool() const { return m_window != nullptr; }

    void* nativeHandle() const {
        if (!m_window) return nullptr;
        return SDL_GetPointerProperty(
            SDL_GetWindowProperties(m_window),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER,
            nullptr);
    }

private:
    SDL_Window* m_window = nullptr;
    bool m_sdlInitialized = false;
};

}
#endif
