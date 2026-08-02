#include "DebugManager.h"
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <numeric>
// no filesystem needed

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <sys/stat.h>
  #include <sys/types.h>
#endif

namespace Caesura {

// Per-frame profile sample cap: PROFILE_SCOPE runs every frame and samples
// were never cleared -- without a cap a long session grows without bound.
// Minimal JSON string escaping for log/report output (quotes, backslash,
// control chars).
static std::string escapeJson(const std::string& s) {
    // Escape via explicit byte comparison to keep the C++ source free of
    // backslash-heavy literals: quotes, backslash, and control chars.
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        if (c == 34) {            // double quote
            out += static_cast<char>(92); out += static_cast<char>(34);
        } else if (c == 92) {     // backslash
            out += static_cast<char>(92); out += static_cast<char>(92);
        } else if (c == 10) {     // LF
            out += static_cast<char>(92); out += 'n';
        } else if (c == 13) {     // CR
            out += static_cast<char>(92); out += 'r';
        } else if (c == 9) {      // TAB
            out += static_cast<char>(92); out += 't';
        } else if (c < 0x20) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", c);
            out += buf;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out;
}

static constexpr size_t kMaxFrameSamples = 4096;

// ============================================================================
//  Utility helpers
// ============================================================================

static std::string formatTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;
    std::tm localTm;
#ifdef _WIN32
    localtime_s(&localTm, &t);
#else
    localtime_r(&t, &localTm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&localTm, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

static std::string formatTimestampFile() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm localTm;
#ifdef _WIN32
    localtime_s(&localTm, &t);
#else
    localtime_r(&t, &localTm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&localTm, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

const char* SubSysName(SubSys s) {
    switch (s) {
        case SubSys::Render:    return "RENDER";
        case SubSys::Audio:     return "AUDIO";
        case SubSys::Scripting: return "SCRIPT";
        case SubSys::Input:     return "INPUT";
        case SubSys::Platform:  return "PLAT";
        case SubSys::Engine:    return "ENGINE";
        case SubSys::Dbg:       return "DEBUG";
    }
    return "UNKNOWN";
}

const char* DbgLevelName(DbgLevel l) {
    switch (l) {
        case DbgLevel::Trace: return "TRACE";
        case DbgLevel::Debug: return "DEBUG";
        case DbgLevel::Info:  return "INFO";
        case DbgLevel::Warn:  return "WARN";
        case DbgLevel::Err:   return "ERROR";
        case DbgLevel::Fatal: return "FATAL";
    }
    return "?";
}

const char* ErrCodeName(ErrCode c) {
    switch (c) {
        case ErrCode::Ok:                          return "OK";
        case ErrCode::Platform_SDL_InitFailed:     return "PLATFORM_SDL_INIT_FAILED";
        case ErrCode::Platform_WindowCreateFailed: return "PLATFORM_WINDOW_CREATE_FAILED";
        case ErrCode::Platform_NativeHandleNull:   return "PLATFORM_NATIVE_HANDLE_NULL";
        case ErrCode::Render_BgfxInitFailed:       return "RENDER_BGFX_INIT_FAILED";
        case ErrCode::Render_ShaderCompileFailed:  return "RENDER_SHADER_COMPILE_FAILED";
        case ErrCode::Render_RTTAllocFailed:       return "RENDER_RTT_ALLOC_FAILED";
        case ErrCode::Render_FramebufferFailed:    return "RENDER_FRAMEBUFFER_FAILED";
        case ErrCode::Render_TextureCreateFailed:  return "RENDER_TEXTURE_CREATE_FAILED";
        case ErrCode::Render_ProgramCreateFailed:  return "RENDER_PROGRAM_CREATE_FAILED";
        case ErrCode::Render_UniformFailed:        return "RENDER_UNIFORM_FAILED";
        case ErrCode::Render_FontAtlasFailed:      return "RENDER_FONT_ATLAS_FAILED";
        case ErrCode::Render_BackendUnknown:       return "RENDER_BACKEND_UNKNOWN";
        case ErrCode::Audio_SoLoudInitFailed:      return "AUDIO_SOLOUD_INIT_FAILED";
        case ErrCode::Audio_FileLoadFailed:        return "AUDIO_FILE_LOAD_FAILED";
        case ErrCode::Audio_BusCreateFailed:       return "AUDIO_BUS_CREATE_FAILED";
        case ErrCode::Audio_VoicePlayFailed:       return "AUDIO_VOICE_PLAY_FAILED";
        case ErrCode::Audio_BGMPlayFailed:         return "AUDIO_BGM_PLAY_FAILED";
        case ErrCode::Script_LuaVMCreateFailed:    return "SCRIPT_LUA_VM_CREATE_FAILED";
        case ErrCode::Script_LoadFailed:           return "SCRIPT_LOAD_FAILED";
        case ErrCode::Script_CoroutineError:       return "SCRIPT_COROUTINE_ERROR";
        case ErrCode::Script_ExecutionError:       return "SCRIPT_EXECUTION_ERROR";
        case ErrCode::Inp_FocusSwitchError:        return "INPUT_FOCUS_SWITCH_ERROR";
        case ErrCode::Engine_PlatformInitFailed:   return "ENGINE_PLATFORM_INIT_FAILED";
        case ErrCode::Engine_RenderInitFailed:     return "ENGINE_RENDER_INIT_FAILED";
        case ErrCode::Engine_AudioInitFailed:      return "ENGINE_AUDIO_INIT_FAILED";
        case ErrCode::Engine_LuaInitFailed:        return "ENGINE_LUA_INIT_FAILED";
        case ErrCode::Engine_UpdateError:          return "ENGINE_UPDATE_ERROR";
        case ErrCode::Engine_RenderError:          return "ENGINE_RENDER_ERROR";
        case ErrCode::Internal_LogFileOpenFailed:  return "INTERNAL_LOG_FILE_OPEN_FAILED";
        case ErrCode::Internal_MutexLockFailed:    return "INTERNAL_MUTEX_LOCK_FAILED";
    }
    return "UNKNOWN_ERROR";
}

// ============================================================================
//  DebugManager ?? singleton
// ============================================================================

DebugManager& DebugManager::instance() {
    static DebugManager mgr;
    return mgr;
}

DebugManager::~DebugManager() {
    shutdown();
}

// -- init -------------------------------------------------------------------

bool DebugManager::init(const char* logDir) {
    // [TD-15] Step-by-step restore to isolate stack overrun
    if (m_initialized) return true;
    if (!logDir) logDir = "logs";  // null -> default log directory
    if (logDir && strstr(logDir, "..")) {
        fprintf(stderr, "[DebugManager] init: path traversal blocked: %s\n", logDir);
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return true;
    {
        std::string _logPath(logDir);
#if defined(_WIN32)
        std::string _accum;
        for (char _c : _logPath) {
            _accum += _c;
            if (_c == '\\' || _c == '/') {
                if (!_accum.empty() && _accum.back() != ':') {
                    CreateDirectoryA(_accum.c_str(), nullptr);
                }
            }
        }
        CreateDirectoryA(_accum.c_str(), nullptr);
#else
        std::string _accum;
        for (char _c : _logPath) {
            _accum += _c;
            if (_c == '/') { mkdir(_accum.c_str(), 0755); }
        }
        mkdir(_accum.c_str(), 0755);
#endif
    }
    m_logFilePath = std::string(logDir) + "/caesura_" + formatTimestampFile() + ".log";
    m_logFile.open(m_logFilePath, std::ios::out | std::ios::trunc);
    if (!m_logFile.is_open()) {
        fprintf(stderr, "[DebugManager] Cannot open log file: %s\n", m_logFilePath.c_str());
        m_initialized = true;
        return true;
    }
    for (const auto& entry : m_ringBuffer) { writeToFile(entry); }
    m_initialized = true;
    fprintf(stderr, "[DebugManager] Logging to: %s\n", m_logFilePath.c_str());
    return true;
}

void DebugManager::shutdown() {
    // Lock order must match init()/log() (m_mutex -> m_ioMutex); a reversed
    // order could deadlock if a concurrent init ever replays the ring buffer
    // while shutdown closes the file.
    std::lock_guard<std::mutex> lock(m_mutex);
    std::lock_guard<std::mutex> ioLock(m_ioMutex);
    if (!m_initialized) return;

    if (m_logFile.is_open()) {
        m_logFile.flush();
        m_logFile.close();
    }
    m_initialized = false;
}

// -- log (with error code) --------------------------------------------------

void DebugManager::log(DbgLevel level, SubSys subsystem, ErrCode code,
                        const char* fmt, ...) {
    char msgBuf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
    va_end(args);

    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level     = level;
    entry.subsystem = subsystem;
    entry.errorCode = code;
    entry.message   = msgBuf;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_ringBuffer.size() >= kRingSize) m_ringBuffer.pop_front();
        m_ringBuffer.push_back(entry);

        size_t idx = static_cast<size_t>(subsystem);
        if (idx < m_totalCounts.size()) m_totalCounts[idx]++;
        if (level == DbgLevel::Err || level == DbgLevel::Fatal) {
            if (idx < m_errorCounts.size()) m_errorCounts[idx]++;
            m_lastErrorEntry = entry;
            m_hasLastError = true;
        }
        if (level == DbgLevel::Warn) {
            if (idx < m_warnCounts.size()) m_warnCounts[idx]++;
        }
    }

    // I/O outside the lock: a slow console/disk must not stall log writers.
    writeToConsole(entry);
    writeToFile(entry);
}

// -- log (without error code) -----------------------------------------------

void DebugManager::log(DbgLevel level, SubSys subsystem, const char* fmt, ...) {
    char msgBuf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
    va_end(args);
    log(level, subsystem, ErrCode::Ok, "%s", msgBuf);
}

// -- writeToConsole ---------------------------------------------------------

void DebugManager::writeToConsole(const LogEntry& entry) {
    std::lock_guard<std::mutex> ioLock(m_ioMutex);
    if (entry.level == DbgLevel::Trace) return;
    FILE* out = (entry.level >= DbgLevel::Err) ? stderr : stdout;

#if defined(_WIN32)
    HANDLE hConsole = GetStdHandle(
        (entry.level >= DbgLevel::Err) ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    WORD color = 7;
    if (entry.level == DbgLevel::Warn) color = 14;
    if (entry.level >= DbgLevel::Err) color = 12;
    SetConsoleTextAttribute(hConsole, color);
#endif

    fprintf(out, "[%s] [%s] %s\n",
            SubSysName(entry.subsystem),
            DbgLevelName(entry.level),
            entry.message.c_str());

#if defined(_WIN32)
    SetConsoleTextAttribute(hConsole, 7);
#endif
}

// -- writeToFile ------------------------------------------------------------

void DebugManager::writeToFile(const LogEntry& entry) {
    std::lock_guard<std::mutex> ioLock(m_ioMutex);
    if (!m_logFile.is_open()) return;
    auto t  = std::chrono::system_clock::to_time_t(entry.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  entry.timestamp.time_since_epoch()) % 1000;
    std::tm localTm;
#ifdef _WIN32
    localtime_s(&localTm, &t);
#else
    localtime_r(&t, &localTm);
#endif
    m_logFile << std::put_time(&localTm, "%H:%M:%S")
              << '.' << std::setfill('0') << std::setw(3) << ms.count()
              << " [" << DbgLevelName(entry.level) << "]"
              << " [" << SubSysName(entry.subsystem) << "]"
              << " " << ErrCodeName(entry.errorCode)
              << " | " << entry.message << "\n";
}

// -- query API --------------------------------------------------------------

LogEntry DebugManager::lastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_hasLastError) return LogEntry{};
    return m_lastErrorEntry;  // copy under lock
}

uint32_t DebugManager::errorCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t total = 0;
    for (auto c : m_errorCounts) total += c;
    return total;
}

uint32_t DebugManager::entryCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<uint32_t>(m_ringBuffer.size());
}

std::deque<LogEntry> DebugManager::ringBuffer() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_ringBuffer;  // snapshot copy under lock
}

uint32_t DebugManager::subsystemErrorCount(SubSys s) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t idx = static_cast<size_t>(s);
    if (idx >= m_errorCounts.size()) return 0;
    return m_errorCounts[idx];
}

// -- Subsystem stats --------------------------------------------------------

SubsystemStats DebugManager::getSubsystemStats(SubSys s) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    SubsystemStats st;
    size_t idx = static_cast<size_t>(s);
    if (idx < m_totalCounts.size()) {
        st.totalCalls = m_totalCounts[idx];
        st.errorCount = m_errorCounts[idx];
        st.warnCount  = m_warnCounts[idx];
    }
    for (auto it = m_ringBuffer.rbegin(); it != m_ringBuffer.rend(); ++it) {
        if (it->subsystem == s && it->level >= DbgLevel::Err) {
            st.lastErrorCode    = static_cast<uint32_t>(it->errorCode);
            st.lastErrorMessage = it->message;
            break;
        }
    }
    return st;
}

// -- dumpFullReport ---------------------------------------------------------

std::string DebugManager::dumpFullReport() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "{" << "\n";
    oss << "  \"engine\": \"Caesura (AmeKAG) v1.0.0\"," << "\n";
    oss << "  \"timestamp\": \"" << formatTimestamp() << "\"," << "\n";
    oss << "  \"total_entries\": " << m_ringBuffer.size() << "," << "\n";
    oss << "  \"total_errors\": " << std::accumulate(
        m_errorCounts.begin(), m_errorCounts.end(), 0u) << "," << "\n";
    oss << "  \"subsystems\": {" << "\n";

    const SubSys subs[] = { SubSys::Render, SubSys::Audio, SubSys::Scripting,
        SubSys::Input, SubSys::Platform, SubSys::Engine, SubSys::Dbg };
    for (size_t i = 0; i < 7; i++) {
        const size_t si = static_cast<size_t>(subs[i]);
        SubsystemStats st;
        st.totalCalls = si < m_totalCounts.size() ? m_totalCounts[si] : 0;
        st.errorCount = si < m_errorCounts.size() ? m_errorCounts[si] : 0;
        st.warnCount  = si < m_warnCounts.size() ? m_warnCounts[si] : 0;
        st.lastErrorMessage.clear();
        for (auto it = m_ringBuffer.rbegin(); it != m_ringBuffer.rend(); ++it) {
            if (it->subsystem == subs[i] && it->level >= DbgLevel::Err) {
                st.lastErrorCode    = static_cast<uint32_t>(it->errorCode);
                st.lastErrorMessage = it->message;
                break;
            }
        }
        oss << "    \"" << SubSysName(subs[i]) << "\": {"
            << "\"calls\":" << st.totalCalls
            << ",\"errors\":" << st.errorCount
            << ",\"warns\":" << st.warnCount
            << ",\"lastErrorMessage\":" << escapeJson(st.lastErrorMessage) << ",\"lastErrorCode\":" << st.lastErrorCode
            << "}";
        if (i < 6) oss << ",";
        oss << "\n";
    }
    oss << "  }," << "\n";
    oss << "  \"render\": {" << "\n"
            << "\"backend\":\"" << m_renderInfo.backendName << "\"" << "\n"
            << ",\"views\":" << m_renderInfo.viewCount
            << ",\"shaderReady\":" << (m_renderInfo.shaderReady ? "true" : "false")
            << "}," << "\n";
    oss << "  \"log_file\": \"" << escapeJson(m_logFilePath) << "\"" << "\n";
    oss << "}" << "\n";
    return oss.str();
}
// -- Mutable info setters/getters -------------------------------------------

void DebugManager::setRenderInfo(const RenderInfo& ri) { m_renderInfo = ri; }
void DebugManager::setAudioInfo(const AudioInfo& ai)   { m_audioInfo  = ai; }
void DebugManager::setInputInfo(const InputInfo& ii)   { m_inputInfo  = ii; }

DebugManager::RenderInfo DebugManager::getRenderInfo() const { return m_renderInfo; }
DebugManager::AudioInfo  DebugManager::getAudioInfo()  const { return m_audioInfo;  }
DebugManager::InputInfo  DebugManager::getInputInfo()  const { return m_inputInfo;  }


// ============================================================================
//  Profiling implementation
// ============================================================================

void DebugManager::beginProfile(const char* label) {
    ProfileSample s;
    s.label = label;
    s.start = std::chrono::high_resolution_clock::now();
    s.depth = m_profileDepth++;
    m_activeSamples.push_back(s);
}

void DebugManager::endProfile(const char* label) {
    if (m_activeSamples.empty()) return;
    auto& s = m_activeSamples.back();
    // W9: label match assertion ?? catch mismatched begin/end pairs
    if (strcmp(s.label, label) != 0) {
        fprintf(stderr, "[Profile] MISMATCH: endProfile(\"%s\") != beginProfile(\"%s\")\n",
                label, s.label);
    }
    auto now = std::chrono::high_resolution_clock::now();
    s.elapsedMs = std::chrono::duration<double, std::milli>(now - s.start).count();
    if (m_frameProfile.samples.size() < kMaxFrameSamples) {
        m_frameProfile.samples.push_back(s);
    }
    m_activeSamples.pop_back();
    m_profileDepth--;
}

void DebugManager::beginFrameProfile() {
    m_frameStart = std::chrono::high_resolution_clock::now();
    m_frameProfile.samples.clear();
    m_frameProfile.gpuSubmitCount = 0;
    m_frameProfile.transientAllocCount = 0;
    m_frameProfile.transientAllocBytes = 0;
}

void DebugManager::endFrameProfile() {
    auto now = std::chrono::high_resolution_clock::now();
    m_frameProfile.totalMs =
        std::chrono::duration<double, std::milli>(now - m_frameStart).count();
}

} // namespace Caesura


