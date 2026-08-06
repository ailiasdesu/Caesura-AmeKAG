// ===========================================================================
//  Caesura (AmeKAG) -- HotReload.h
//  Phase 8.1: File monitoring + coroutine rebuild for .ks/.lua scripts.
//  Engine-owned file monitor, called per-frame in Engine::run().
// ===========================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <chrono>

struct lua_State;

namespace Caesura {

enum class ScriptState {
    IDLE,          // Running normally
    DEBUG_ACTIVE,  // Debugger has stopped execution
    RELOADING      // Hot reload in progress (blocking)
};

class HotReload {
public:
    HotReload() = default;
    ~HotReload() = default;

    HotReload(const HotReload&) = delete;
    HotReload& operator=(const HotReload&) = delete;

    // Initialize -- scan scriptDir recursively for .ks/.lua files.
    // Stores initial last_write_time for each file.
    void init(const std::string& scriptDir, lua_State* L);

    // Add another directory to the watch set (e.g. assets/script/ for
    // scene files, mods/ for mod content). Scanned immediately.
    void addWatchRoot(const std::string& dir);

    void shutdown();

    // Per-frame check. Returns true if a reload was triggered.
    // On change: cancel all active ops → coroutine.close() →
    //            GameState reset → reload scripts → show warning.
    bool checkAndReload();

    // Accessors
    ScriptState scriptState() const { return m_scriptState; }
    void setScriptState(ScriptState s) { m_scriptState = s; }
    const std::string& scriptDir() const { return m_scriptDir; }
    bool initialized() const { return m_initialized; }

    // Force a reload next frame (used by ErrorUI retry)
    void requestReload() { m_reloadRequested = true; }

private:
    void scanDirectory();
    void showWarningOverlay(const std::string& message);

    // Scan throttling: filesystem polling every frame (60-160 stat syscalls
    // per frame) was a measurable hot path; scans run at most every
    // kScanIntervalMs.
    static constexpr long long kScanIntervalMs = 500;
    long long m_lastScanMs = -1;

    std::string                m_scriptDir;
    std::vector<std::string>   m_watchRoots;
    lua_State*                 m_L = nullptr;
    bool                       m_initialized = false;
    ScriptState                m_scriptState = ScriptState::IDLE;
    bool                       m_reloadRequested = false;
    int                        m_warningFrames = 0;
    std::string                m_warningText;

    // file path → last_write_time
    using Clock = std::filesystem::file_time_type;
    std::unordered_map<std::string, Clock> m_fileTimes;
};

} // namespace Caesura
