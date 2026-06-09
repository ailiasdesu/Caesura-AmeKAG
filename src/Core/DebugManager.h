#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <fstream>
#include <mutex>
#include <array>
#include <deque>

// ============================================================================
//  CAESURA_DEBUG compilation gate
// ============================================================================
#ifndef CAESURA_DEBUG
  #define CAESURA_DEBUG 1
#endif






// -- Convenience macros (Windows-safe -- no ERROR/OK/INFO tokens) ------------

#if CAESURA_DEBUG
  #define DEBUG_TRACE(ss, ec, fmt, ...) \
    Caesura::DebugManager::instance().log((Caesura::DbgLevel)0, ss, ec, fmt, ##__VA_ARGS__)
  #define DEBUG_DBG(ss, ec, fmt, ...) \
    Caesura::DebugManager::instance().log((Caesura::DbgLevel)1, ss, ec, fmt, ##__VA_ARGS__)
  #define DEBUG_INFO(ss, ec, fmt, ...) \
    Caesura::DebugManager::instance().log((Caesura::DbgLevel)2, ss, ec, fmt, ##__VA_ARGS__)
#else
  #define DEBUG_TRACE(...)  ((void)0)
  #define DEBUG_DBG(...)    ((void)0)
  #define DEBUG_INFO(...)   ((void)0)
#endif

#define DEBUG_WARN(ss, ec, fmt, ...) \
  Caesura::DebugManager::instance().log((Caesura::DbgLevel)3, ss, ec, fmt, ##__VA_ARGS__)
#define DEBUG_ERR(ss, ec, fmt, ...) \
  Caesura::DebugManager::instance().log((Caesura::DbgLevel)4, ss, ec, fmt, ##__VA_ARGS__)
#define DEBUG_FATAL(ss, ec, fmt, ...) \
  Caesura::DebugManager::instance().log((Caesura::DbgLevel)5, ss, ec, fmt, ##__VA_ARGS__)

// Backward-compat aliases
#define DEBUG_ERROR DEBUG_ERR
#define DEBUG_DEBUG DEBUG_DBG

// -- Profiling macros (instrumentation, compiled-in always) -----------------

#define PROFILE_BEGIN(label) \
    Caesura::DebugManager::instance().beginProfile(label)

#define PROFILE_END(label) \
    Caesura::DebugManager::instance().endProfile(label)

#define PROFILE_CONCAT2(a,b) a##b
#define PROFILE_CONCAT(a,b)  PROFILE_CONCAT2(a,b)
#define PROFILE_SCOPE(label) \
    Caesura::DebugManager::instance().beginProfile(label); \
    struct PROFILE_CONCAT(_pshCleanup_,__LINE__) { \
        ~PROFILE_CONCAT(_pshCleanup_,__LINE__)() { \
            Caesura::DebugManager::instance().endProfile(label); \
        } \
    } PROFILE_CONCAT(_psh_,__LINE__)

namespace Caesura {

// ============================================================================
//  Enums  (inside Caesura namespace to avoid Windows macro collisions)
// ============================================================================
enum class DbgLevel : uint8_t {
    Trace = 0, Debug = 1, Info = 2, Warn = 3, Err = 4, Fatal = 5,
};
enum class SubSys : uint8_t {
    Render = 0, Audio = 1, Scripting = 2, Input = 3, Platform = 4, Engine = 5, Dbg = 6,
};
enum class ErrCode : uint32_t {
    Ok = 0,
    Platform_SDL_InitFailed = 1001, Platform_WindowCreateFailed = 1002, Platform_NativeHandleNull = 1003,
    Render_BgfxInitFailed = 2001, Render_ShaderCompileFailed = 2002, Render_RTTAllocFailed = 2003,
    Render_FramebufferFailed = 2004, Render_TextureCreateFailed = 2005, Render_ProgramCreateFailed = 2006,
    Render_UniformFailed = 2007, Render_FontAtlasFailed = 2008, Render_BackendUnknown = 2009,
    Audio_SoLoudInitFailed = 3001, Audio_FileLoadFailed = 3002, Audio_BusCreateFailed = 3003,
    Audio_VoicePlayFailed = 3004, Audio_BGMPlayFailed = 3005,
    Script_LuaVMCreateFailed = 4001, Script_LoadFailed = 4002, Script_CoroutineError = 4003, Script_ExecutionError = 4004,
    Inp_FocusSwitchError = 5001,
    Engine_PlatformInitFailed = 6001, Engine_RenderInitFailed = 6002, Engine_AudioInitFailed = 6003,
    Engine_LuaInitFailed = 6004, Engine_UpdateError = 6005, Engine_RenderError = 6006,
    Internal_LogFileOpenFailed = 9001, Internal_MutexLockFailed = 9002,
};
const char* SubSysName(SubSys s);
const char* DbgLevelName(DbgLevel l);
const char* ErrCodeName(ErrCode c);


// ============================================================================
//  Profiling
// ============================================================================
struct ProfileSample {
    const char* label;
    std::chrono::high_resolution_clock::time_point start;
    double elapsedMs = 0.0;
    int depth = 0;
};

struct FrameProfile {
    double totalMs     = 0.0;
    double gpuSubmitMs = 0.0;
    double luaGcMs     = 0.0;
    uint32_t gpuSubmitCount    = 0;
    uint32_t transientAllocCount = 0;
    uint32_t transientAllocBytes = 0;
    std::vector<ProfileSample> samples;
};

// ============================================================================
//  LogEntry
// ============================================================================
struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    DbgLevel    level;
    SubSys      subsystem;
    ErrCode     errorCode;
    std::string message;
};

// ============================================================================
//  SubsystemStats
// ============================================================================
struct SubsystemStats {
    uint32_t totalCalls       = 0;
    uint32_t errorCount       = 0;
    uint32_t warnCount        = 0;
    uint32_t lastErrorCode    = 0;
    std::string lastErrorMessage;
    std::string extra;
};

// ============================================================================
//  DebugManager
// ============================================================================
class DebugManager {
public:
    static DebugManager& instance();
    DebugManager(const DebugManager&) = delete;
    DebugManager& operator=(const DebugManager&) = delete;

    bool init(const char* logDir = "logs");
    void shutdown();

    void log(DbgLevel level, SubSys subsystem, ErrCode code, const char* fmt, ...);
    void log(DbgLevel level, SubSys subsystem, const char* fmt, ...);

    const LogEntry* lastError() const;
    uint32_t errorCount() const;
    uint32_t entryCount() const;
    uint32_t subsystemErrorCount(SubSys s) const;
    const std::deque<LogEntry>& ringBuffer() const { return m_ringBuffer; }

    SubsystemStats getSubsystemStats(SubSys s) const;
    std::string    dumpFullReport();

    // -- Profiling ---------------------------------------------------------
    void beginProfile(const char* label);
    void endProfile(const char* label);
    void recordGpuSubmit(uint32_t count)   { m_frameProfile.gpuSubmitCount += count; }
    void recordTransientAlloc(uint32_t count, uint32_t bytes) {
        m_frameProfile.transientAllocCount += count;
        m_frameProfile.transientAllocBytes += bytes;
    }
    void recordLuaGc(double ms)            { m_frameProfile.luaGcMs += ms; }
    const FrameProfile& getFrameProfile() const { return m_frameProfile; }
    void endFrameProfile();

    struct RenderInfo {
        std::string backendName;
        int width  = 0, height = 0, viewCount = 3, textureCount = 0, rttCount = 0;
        bool shaderReady = false;
    };
    RenderInfo getRenderInfo() const;
    struct AudioInfo {
        bool initialized = false;
        int activeVoices = 0, activeBGM = 0;
        float globalVolume = 1.0f;
        bool bgmBusReady = false, voiceBusReady = false, seBusReady = false;
    };
    AudioInfo getAudioInfo() const;
    struct InputInfo {
        std::string currentFocus;
        int kagCallbackCount = 0, gameCallbackCount = 0;
        bool clickPending = false;
    };
    InputInfo getInputInfo() const;

    void setRenderInfo(const RenderInfo& ri);
    void setAudioInfo(const AudioInfo& ai);
    void setInputInfo(const InputInfo& ii);
    const std::string& logFilePath() const { return m_logFilePath; }

private:
    DebugManager() = default;
    ~DebugManager();
    void writeToFile(const LogEntry& entry);
    void writeToConsole(const LogEntry& entry);

    bool m_initialized = false;
    std::mutex m_mutex;
    std::ofstream m_logFile;
    std::string m_logFilePath;

    static constexpr size_t kRingSize = 1024;
    std::deque<LogEntry> m_ringBuffer;
    std::array<uint32_t, 7> m_errorCounts = {}, m_warnCounts = {}, m_totalCounts = {};
    LogEntry m_lastErrorEntry;
    bool m_hasLastError = false;
    RenderInfo m_renderInfo;
    AudioInfo  m_audioInfo;
    InputInfo  m_inputInfo;

    // Profiling
    FrameProfile m_frameProfile;
    std::chrono::high_resolution_clock::time_point m_frameStart;
    int m_profileDepth = 0;
    std::vector<ProfileSample> m_activeSamples;
};

} // namespace Caesura