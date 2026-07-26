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

    std::string                m_scriptDir;
    lua_State*                 m_L = nullptr;
    bool                       m_initialized = false;
    ScriptState                m_scriptState = ScriptState::IDLE;
    bool                       m_reloadRequested = false;
    int                        m_warningFrames = 0;

    // file path → last_write_time
    using Clock = std::filesystem::file_time_type;
    std::unordered_map<std::string, Clock> m_fileTimes;
};

} // namespace Caesura
