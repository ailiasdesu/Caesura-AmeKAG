// ===========================================================================
//  Caesura (AmeKAG) -- DebugProtocol.cpp
//  Phase 8.2: Lua debug hooks for breakpoints, stepping, and inspection.
// ===========================================================================

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include "DebugProtocol.h"
#include "HotReload.h"
#include "../Core/DebugManager.h"
#include <thread>
#include <chrono>
#include <cstdio>
#include <sstream>

namespace Caesura {

DebugProtocol& DebugProtocol::instance() {
    static DebugProtocol s;
    return s;
}

void DebugProtocol::init(lua_State* L) {
    if (m_initialized) return;

    m_L = L;
    m_initialized = true;

    // Register the hook callback with LUA_MASKLINE
    lua_sethook(L, hookCallback, LUA_MASKLINE, 0);

    DEBUG_INFO(SubSys::Dbg, ErrCode::Ok,
               "DebugProtocol initialized -- lua_sethook registered.");
}

// -- Breakpoint management ------------------------------------------------

static std::string breakpointKey(const std::string& file, int line) {
    return file + ":" + std::to_string(line);
}

void DebugProtocol::setBreakpoint(const std::string& file, int line) {
    if (!m_initialized) return;
    m_breakpoints.insert(breakpointKey(file, line));
    DEBUG_INFO(SubSys::Dbg, ErrCode::Ok,
               "Breakpoint set: %s:%d", file.c_str(), line);
}

void DebugProtocol::removeBreakpoint(const std::string& file, int line) {
    if (!m_initialized) return;
    m_breakpoints.erase(breakpointKey(file, line));
}

void DebugProtocol::clearAllBreakpoints() {
    m_breakpoints.clear();
}

bool DebugProtocol::hasBreakpoint(const std::string& file, int line) const {
    return m_breakpoints.count(breakpointKey(file, line)) > 0;
}

// -- Execution control ----------------------------------------------------

void DebugProtocol::stepOver() {
    m_stepMode = StepMode::Over;
    // Get current call depth from lua_Debug
    lua_Debug ar;
    if (lua_getstack(m_L, 0, &ar)) {
        lua_getinfo(m_L, "Sln", &ar);
        // ar contains current line info
    }
    m_stepDepth = 0;  // Will be set in hook if currently at a break
    m_waitingForCommand = false;
    DEBUG_INFO(SubSys::Dbg, ErrCode::Ok, "Debug: stepOver");
}

void DebugProtocol::stepInto() {
    m_stepMode = StepMode::Into;
    m_stepDepth = 0;
    m_waitingForCommand = false;
    DEBUG_INFO(SubSys::Dbg, ErrCode::Ok, "Debug: stepInto");
}

void DebugProtocol::stepOut() {
    m_stepMode = StepMode::Out;
    // Record current call depth
    lua_Debug ar;
    int depth = 0;
    while (lua_getstack(m_L, depth + 1, &ar)) depth++;
    m_stepOutTargetDepth = depth - 1;
    m_waitingForCommand = false;
    DEBUG_INFO(SubSys::Dbg, ErrCode::Ok, "Debug: stepOut (target depth %d)", m_stepOutTargetDepth);
}

void DebugProtocol::continue_() {
    m_stepMode = StepMode::None;
    m_waitingForCommand = false;
    DEBUG_INFO(SubSys::Dbg, ErrCode::Ok, "Debug: continue");
}

// -- Inspection -----------------------------------------------------------

std::string DebugProtocol::inspectLocal(int frameIndex, const std::string& name) {
    if (!m_L) return "nil";

    lua_Debug ar;
    if (!lua_getstack(m_L, frameIndex, &ar)) return "<invalid frame>";

    const char* cname = name.empty() ? nullptr : name.c_str();
    const char* val = lua_getlocal(m_L, &ar, 1);
    while (val != nullptr) {
        if (cname && strcmp(val, cname) == 0) {
            std::string result = formatValue(m_L, -1);
            lua_pop(m_L, 1);
            return result;
        }
        lua_pop(m_L, 1);
        int idx = 2;
        while ((val = lua_getlocal(m_L, &ar, idx++)) != nullptr) {
            if (cname && strcmp(val, cname) == 0) {
                std::string result = formatValue(m_L, -1);
                lua_pop(m_L, 1);
                return result;
            }
            lua_pop(m_L, 1);
        }
        break;
    }

    return "nil";
}

std::string DebugProtocol::inspectGlobal(const std::string& name) {
    if (!m_L || name.empty()) return "nil";

    lua_getglobal(m_L, name.c_str());
    std::string result = formatValue(m_L, -1);
    lua_pop(m_L, 1);
    return result;
}

// -- Hook callback --------------------------------------------------------

void DebugProtocol::hookCallback(lua_State* L, lua_Debug* ar) {
    auto& dp = instance();
    if (!dp.m_initialized) return;

    // Get source info
    lua_getinfo(L, "Sln", ar);
    const char* source = ar->source ? ar->source : "?";
    int currentLine = ar->currentline;

    dp.m_currentSource = source;
    dp.m_currentLine = currentLine;

    // Check if we're in the middle of a reload -- don't interrupt
    auto& hr = HotReload::instance();
    if (hr.scriptState() == ScriptState::RELOADING) return;

    bool shouldBreak = false;

    switch (dp.m_stepMode) {
        case StepMode::Into:
            shouldBreak = true;
            break;
        case StepMode::Over:
            if (dp.m_stepDepth == 0) {
                // First hit -- record depth
                dp.m_stepDepth = 1;  // Will be tracked via stack depth
                shouldBreak = true;
            }
            // Actually track this properly:
            // stepOver should break on the next line at the same or shallower depth
            break;
        case StepMode::Out: {
            int depth = 0;
            lua_Debug ar2;
            while (lua_getstack(L, depth + 1, &ar2)) depth++;
            if (depth <= dp.m_stepOutTargetDepth) {
                dp.m_stepMode = StepMode::None;
                shouldBreak = true;
            }
            return;  // Don't block on step-out frames, just return
        }
        case StepMode::None:
            // Check breakpoints
            if (dp.hasBreakpoint(source, currentLine)) {
                shouldBreak = true;
            }
            break;
    }

    if (shouldBreak) {
        dp.m_stepMode = StepMode::None;
        dp.m_waitingForCommand = true;

        fprintf(stderr, "[Debug] Hit at %s:%d\n", source, currentLine);

        // Block until user command (sleep 50ms loop)
        dp.waitForCommand();
    }
}

void DebugProtocol::waitForCommand() {
    using namespace std::chrono_literals;
    while (m_waitingForCommand.load()) {
        // Also check if reload was requested
        auto& hr = HotReload::instance();
        if (hr.scriptState() == ScriptState::RELOADING) {
            m_waitingForCommand = false;
            m_stepMode = StepMode::None;
            return;
        }
        std::this_thread::sleep_for(50ms);
    }
}

// -- Value formatting -----------------------------------------------------

std::string DebugProtocol::formatValue(lua_State* L, int index) {
    int t = lua_type(L, index);
    switch (t) {
        case LUA_TNIL:
            return "nil";
        case LUA_TBOOLEAN:
            return lua_toboolean(L, index) ? "true" : "false";
        case LUA_TNUMBER: {
            std::ostringstream oss;
            oss << lua_tonumber(L, index);
            return oss.str();
        }
        case LUA_TSTRING:
            return std::string("\"") + lua_tostring(L, index) + "\"";
        case LUA_TTABLE: {
            std::ostringstream oss;
            oss << "table(";
            lua_len(L, index);
            int len = (int)lua_tointeger(L, -1);
            lua_pop(L, 1);
            oss << len << " entries)";
            return oss.str();
        }
        case LUA_TFUNCTION:
            return "<function>";
        case LUA_TTHREAD:
            return "<thread>";
        case LUA_TUSERDATA:
            return "<userdata>";
        case LUA_TLIGHTUSERDATA:
            return "<lightuserdata>";
        default:
            return "<unknown>";
    }
}

} // namespace Caesura