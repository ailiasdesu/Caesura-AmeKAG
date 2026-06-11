#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <deque>

namespace Caesura {

// ============================================================================
//  Enums — shared between macros (DebugManager.h) and IDebugManager
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
//  Shared structs
// ============================================================================

struct ProfileSample {
    const char* label = nullptr;
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

struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    DbgLevel    level = DbgLevel::Info;
    SubSys      subsystem = SubSys::Dbg;
    ErrCode     errorCode = ErrCode::Ok;
    std::string message;
};

struct SubsystemStats {
    uint32_t totalCalls       = 0;
    uint32_t errorCount       = 0;
    uint32_t warnCount        = 0;
    uint32_t lastErrorCode    = 0;
    std::string lastErrorMessage;
    std::string extra;
};

// ============================================================================
// IDebugManager — pure virtual interface for debug/logging subsystem
// ============================================================================
// DebugManager implements this interface. BackendRegistry stores IDebugManager*
// so it does not depend on the concrete singleton implementation.
//
// The DEBUG_* macros in DebugManager.h call DebugManager::instance() directly
// for zero-overhead logging; they are NOT routed through this interface.
// This interface exists for architectural decoupling and testability.

class IDebugManager {
public:
    virtual ~IDebugManager() = default;

    virtual bool init(const char* logDir = "logs") = 0;
    virtual void shutdown() = 0;

    virtual void log(DbgLevel level, SubSys subsystem, ErrCode code, const char* fmt, ...) = 0;
    virtual void log(DbgLevel level, SubSys subsystem, const char* fmt, ...) = 0;

    virtual const LogEntry* lastError() const = 0;
    virtual uint32_t errorCount() const = 0;
    virtual uint32_t entryCount() const = 0;
    virtual uint32_t subsystemErrorCount(SubSys s) const = 0;
    virtual const std::deque<LogEntry>& ringBuffer() const = 0;

    virtual SubsystemStats getSubsystemStats(SubSys s) const = 0;
    virtual std::string    dumpFullReport() = 0;

    // -- Profiling ---------------------------------------------------------
    virtual void beginProfile(const char* label) = 0;
    virtual void endProfile(const char* label) = 0;
    virtual void recordGpuSubmit(uint32_t count) = 0;
    virtual void recordTransientAlloc(uint32_t count, uint32_t bytes) = 0;
    virtual void recordLuaGc(double ms) = 0;
    virtual const FrameProfile& getFrameProfile() const = 0;
    virtual void endFrameProfile() = 0;

    // -- Subsystem info structs (owned by the interface) --------------------
    struct RenderInfo {
        std::string backendName;
        int width = 0, height = 0, viewCount = 3, textureCount = 0, rttCount = 0;
        bool shaderReady = false;
    };
    struct AudioInfo {
        bool initialized = false;
        int activeVoices = 0, activeBGM = 0;
        float globalVolume = 1.0f;
        bool bgmBusReady = false, voiceBusReady = false, seBusReady = false;
    };
    struct InputInfo {
        std::string currentFocus;
        int kagCallbackCount = 0, gameCallbackCount = 0;
        bool clickPending = false;
    };

    virtual RenderInfo getRenderInfo() const = 0;
    virtual AudioInfo  getAudioInfo() const = 0;
    virtual InputInfo  getInputInfo() const = 0;
    virtual void setRenderInfo(const RenderInfo& ri) = 0;
    virtual void setAudioInfo(const AudioInfo& ai) = 0;
    virtual void setInputInfo(const InputInfo& ii) = 0;
    virtual const std::string& logFilePath() const = 0;
};

} // namespace Caesura
