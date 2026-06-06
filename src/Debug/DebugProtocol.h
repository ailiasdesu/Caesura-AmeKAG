// ===========================================================================
//  Caesura (AmeKAG) — DebugProtocol.h
//  Phase 8.2: Lua debug hooks for breakpoints, stepping, and inspection.
//  Uses lua_sethook with LUA_MASKLINE for breakpoint detection.
// ===========================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include "HotReload.h"  // for ScriptState

struct lua_State;
struct lua_Debug;

namespace Caesura {

// Forward
class HotReload;

class DebugProtocol {
public:
    static DebugProtocol& instance();

    DebugProtocol(const DebugProtocol&) = delete;
    DebugProtocol& operator=(const DebugProtocol&) = delete;

    // Initialize with the main Lua state
    void init(lua_State* L);

    // Breakpoints
    void setBreakpoint(const std::string& file, int line);
    void removeBreakpoint(const std::string& file, int line);
    void clearAllBreakpoints();
    bool hasBreakpoint(const std::string& file, int line) const;

    // Execution control (called from UI/remote)
    void stepOver();
    void stepInto();
    void stepOut();
    void continue_();

    // Inspection
    std::string inspectLocal(int frameIndex, const std::string& name);
    std::string inspectGlobal(const std::string& name);
    std::string currentSource() const { return m_currentSource; }
    int currentLine() const { return m_currentLine; }
    bool isDebugActive() const { return m_waitingForCommand; }

    // Hook callback — registered via lua_sethook
    static void hookCallback(lua_State* L, lua_Debug* ar);

private:
    DebugProtocol() = default;
    ~DebugProtocol() = default;

    // Block until a command is received (sleep 50ms loop)
    void waitForCommand();

    // Format a Lua value at the given stack index
    static std::string formatValue(lua_State* L, int index);

    lua_State* m_L = nullptr;
    bool m_initialized = false;

    // Breakpoint storage: "file:line" → set
    std::unordered_set<std::string> m_breakpoints;

    // Current debug state
    std::atomic<bool> m_waitingForCommand{false};
    std::string m_currentSource;
    int m_currentLine = 0;

    // Stepping state
    enum class StepMode { None, Into, Over, Out };
    StepMode m_stepMode = StepMode::None;
    int m_stepDepth = 0;  // call depth at step initiation

    // Step-out tracking
    int m_stepOutTargetDepth = 0;
};

} // namespace Caesura