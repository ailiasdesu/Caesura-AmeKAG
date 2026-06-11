#pragma once

#include "api/IDebugManager.h"
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
  #define CAESURA_DEBUG 0
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
//  DebugManager — implements IDebugManager
// ============================================================================
// Enums (DbgLevel, SubSys, ErrCode) and shared structs (ProfileSample,
// FrameProfile, LogEntry, SubsystemStats) now live in api/IDebugManager.h.
//
// The macros above call DebugManager::instance() directly — they do NOT go
// through IDebugManager. This preserves zero-overhead logging while allowing
// BackendRegistry to depend only on the abstract interface.

class DebugManager : public IDebugManager {
public:
    // Backward-compat aliases (structs now owned by IDebugManager)
    using RenderInfo = IDebugManager::RenderInfo;
    using AudioInfo  = IDebugManager::AudioInfo;
    using InputInfo  = IDebugManager::InputInfo;

    static DebugManager& instance();
    DebugManager(const DebugManager&) = delete;
    DebugManager& operator=(const DebugManager&) = delete;

    bool init(const char* logDir = "logs") override;
    void shutdown() override;

    void log(DbgLevel level, SubSys subsystem, ErrCode code, const char* fmt, ...) override;
    void log(DbgLevel level, SubSys subsystem, const char* fmt, ...) override;

    const LogEntry* lastError() const override;
    uint32_t errorCount() const override;
    uint32_t entryCount() const override;
    uint32_t subsystemErrorCount(SubSys s) const override;
    const std::deque<LogEntry>& ringBuffer() const override { return m_ringBuffer; }

    SubsystemStats getSubsystemStats(SubSys s) const override;
    std::string    dumpFullReport() override;

    // -- Profiling ---------------------------------------------------------
    void beginProfile(const char* label) override;
    void endProfile(const char* label) override;
    void recordGpuSubmit(uint32_t count) override { m_frameProfile.gpuSubmitCount += count; }
    void recordTransientAlloc(uint32_t count, uint32_t bytes) override {
        m_frameProfile.transientAllocCount += count;
        m_frameProfile.transientAllocBytes += bytes;
    }
    void recordLuaGc(double ms) override { m_frameProfile.luaGcMs += ms; }
    const FrameProfile& getFrameProfile() const override { return m_frameProfile; }
    void endFrameProfile() override;

    RenderInfo getRenderInfo() const override;
    AudioInfo  getAudioInfo() const override;
    InputInfo  getInputInfo() const override;

    void setRenderInfo(const RenderInfo& ri) override;
    void setAudioInfo(const AudioInfo& ai) override;
    void setInputInfo(const InputInfo& ii) override;
    const std::string& logFilePath() const override { return m_logFilePath; }

private:
    DebugManager() = default;
    ~DebugManager() override;
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
    IDebugManager::RenderInfo m_renderInfo;
    IDebugManager::AudioInfo  m_audioInfo;
    IDebugManager::InputInfo  m_inputInfo;

    // Profiling
    FrameProfile m_frameProfile;
    std::chrono::high_resolution_clock::time_point m_frameStart;
    int m_profileDepth = 0;
    std::vector<ProfileSample> m_activeSamples;
};

} // namespace Caesura
