// ===========================================================================
//  Caesura (AmeKAG) -- HotReload.cpp
//  Phase 8.1: File monitoring + coroutine rebuild for .ks/.lua scripts.
// ===========================================================================

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include "HotReload.h"
#include "DebugManager.h"
#include <bgfx/bgfx.h>
#include <SDL3/SDL.h>
#include <chrono>
#include <thread>
#include <cstdio>

namespace Caesura {

void HotReload::init(const std::string& scriptDir, lua_State* L) {
    m_scriptDir = scriptDir;
    m_L = L;
    m_fileTimes.clear();
    m_scriptState = ScriptState::IDLE;
    m_reloadRequested = false;
    m_warningFrames = 0;

    m_watchRoots.clear();
    m_watchRoots.push_back(scriptDir);
    scanDirectory();

    m_initialized = true;
    DEBUG_INFO(SubSys::Dbg, ErrCode::Ok,
               "HotReload initialized -- monitoring %zu files in %s",
               m_fileTimes.size(), scriptDir.c_str());
}

void HotReload::addWatchRoot(const std::string& dir) {
    if (dir.empty()) return;
    m_watchRoots.push_back(dir);
    scanDirectory();
    DEBUG_INFO(SubSys::Dbg, ErrCode::Ok,
               "HotReload: added watch root %s (total %zu files)",
               dir.c_str(), m_fileTimes.size());
}

void HotReload::shutdown() {
    m_L = nullptr;
    m_scriptDir.clear();
    m_fileTimes.clear();
    m_initialized = false;
    m_scriptState = ScriptState::IDLE;
    m_reloadRequested = false;
    m_warningFrames = 0;
}

void HotReload::scanDirectory() {
    if (m_scriptDir.empty()) return;

    namespace fs = std::filesystem;
    try {
        for (const auto& root : m_watchRoots) {
            if (root.empty()) continue;
            for (const auto& entry : fs::recursive_directory_iterator(root)) {
                if (!entry.is_regular_file()) continue;
                auto ext = entry.path().extension().string();
                if (ext == ".ks" || ext == ".lua") {
                    m_fileTimes[entry.path().string()] = entry.last_write_time();
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        DEBUG_WARN(SubSys::Dbg, ErrCode::Internal_LogFileOpenFailed,
                   "HotReload scan failed: %s", e.what());
    }
}

// Helper: push GameState ctx table from Lua registry (avoids script/ dependency)
static bool pushGameState(lua_State* L) {
    if (!L) return false;
    if (lua_getfield(L, LUA_REGISTRYINDEX, "caesura_ctx") == LUA_TNIL) {
        lua_pop(L, 1);
        return false;
    }
    return true;
}

// Helper: cancel all active operations stored in ctx.active_operations.
// Each entry is expected to be a CancelToken table with :mark_cancelled()
// and :execute_callbacks().
static void cancelAllActiveOps(lua_State* L) {
    if (!L) return;

    // Push ctx from registry
    if (!pushGameState(L)) return;  // stack: ctx

    lua_getfield(L, -1, "active_operations");  // stack: ctx, ops
    if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        return;
    }

    // Iterate: Phase 1 -- mark_cancelled on all
    int opsLen = (int)lua_rawlen(L, -1);
    for (int i = 1; i <= opsLen; i++) {
        lua_rawgeti(L, -1, i);  // stack: ctx, ops, op
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "mark_cancelled");
            if (lua_isfunction(L, -1)) {
                lua_pushvalue(L, -2);  // push self
                if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                    fprintf(stderr, "[HotReload] mark_cancelled failed: %s\n",
                            lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown");
                    lua_pop(L, 1);
                }
            } else { lua_pop(L, 1); }
        }
        lua_pop(L, 1);  // pop op
    }

    // Phase 2 -- execute_callbacks on all (reverse order not critical here)
    for (int i = 1; i <= opsLen; i++) {
        lua_rawgeti(L, -1, i);  // stack: ctx, ops, op
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "execute_callbacks");
            if (lua_isfunction(L, -1)) {
                lua_pushvalue(L, -2);  // push self
                if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                    fprintf(stderr, "[HotReload] execute_callbacks failed: %s\n",
                            lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown");
                    lua_pop(L, 1);
                }
            } else { lua_pop(L, 1); }
        }
        lua_pop(L, 1);  // pop op
    }

    // Clear active_operations
    lua_newtable(L);
    lua_setfield(L, -3, "active_operations");

    lua_pop(L, 2);  // pop ops, ctx
}

bool HotReload::checkAndReload() {
    if (!m_initialized || !m_L) return false;
    // A paused coroutine is strongly anchored by DebugProtocol. Defer both
    // file scanning and forced reloads until that protocol resumes or detaches.
    if (m_scriptState == ScriptState::DEBUG_ACTIVE) return false;

    // Throttle: do not stat the whole scripts/ tree every frame. A forced
    // reload request bypasses the interval.
    const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (!m_reloadRequested && m_lastScanMs >= 0 &&
        (now - m_lastScanMs) < kScanIntervalMs) {
        return false;
    }
    m_lastScanMs = now;

    // Check for changes
    bool changed = m_reloadRequested;
    m_reloadRequested = false;
    std::vector<std::string> changedKs;

    if (!changed) {
        namespace fs = std::filesystem;
        for (auto& [path, lastTime] : m_fileTimes) {
            std::error_code ec;
            auto currentTime = fs::last_write_time(path, ec);
            if (ec) continue;
            if (currentTime > lastTime) {
                lastTime = currentTime;
                changed = true;
                if (path.size() >= 3
                    && path.compare(path.size() - 3, 3, ".ks") == 0) {
                    changedKs.push_back(path);
                }
                DEBUG_INFO(SubSys::Dbg, ErrCode::Ok,
                           "HotReload: change detected in %s", path.c_str());
            }
        }
    }

    if (!changed) {
        // Check for new files
        namespace fs = std::filesystem;
        try {
            for (const auto& root : m_watchRoots) {
                for (const auto& entry : fs::recursive_directory_iterator(root)) {
                    if (!entry.is_regular_file()) continue;
                    auto ext = entry.path().extension().string();
                    if (ext == ".ks" || ext == ".lua") {
                        auto path = entry.path().string();
                        if (m_fileTimes.find(path) == m_fileTimes.end()) {
                            m_fileTimes[path] = entry.last_write_time();
                            changed = true;
                            if (ext == ".ks") changedKs.push_back(path);
                            DEBUG_INFO(SubSys::Dbg, ErrCode::Ok,
                                       "HotReload: new file detected: %s", path.c_str());
                        }
                    }
                }
            }
        } catch (...) {}
    }

    if (!changed) {
        if (m_warningFrames > 0) {
            m_warningFrames--;
            showWarningOverlay(m_warningText);
        }
        return false;
    }

    // -- Surgical .ks reload -------------------------------------------------
    // A scene file (.ks) changed and NO .lua changed: live re-parse the
    // scene through kag_runner.reload_scene (mods-aware, cache-busting,
    // position remapped, game state preserved) instead of the nuclear
    // reset. The runner's Lua hook reports success/failure; a failure
    // (parse error in the edited file) leaves the game running untouched.
    if (!changedKs.empty()) {
        int reloaded = 0;
        for (const auto& path : changedKs) {
            lua_getglobal(m_L, "require");
            lua_pushstring(m_L, "kag_runner");
            if (lua_pcall(m_L, 1, 1, 0) != LUA_OK || !lua_istable(m_L, -1)) {
                const char* err = lua_tostring(m_L, -1);
                fprintf(stderr, "[HotReload] kag_runner require failed: %s\n",
                        err ? err : "(unknown)");
                lua_settop(m_L, 0);
                break;
            }
            lua_getfield(m_L, -1, "reload_scene");
            if (lua_isfunction(m_L, -1)) {
                lua_pushvalue(m_L, -2);  // self = module
                lua_pushstring(m_L, path.c_str());
                if (lua_pcall(m_L, 2, 1, 0) == LUA_OK) {
                    const char* r = lua_tostring(m_L, -1);
                    DEBUG_INFO(SubSys::Dbg, ErrCode::Ok,
                               "HotReload: scene reload %s -> %s",
                               path.c_str(), r ? r : "?");
                    ++reloaded;
                } else {
                    const char* err = lua_tostring(m_L, -1);
                    fprintf(stderr, "[HotReload] scene reload failed for %s: %s\n",
                            path.c_str(), err ? err : "(unknown)");
                }
            }
            lua_settop(m_L, 0);  // pop fn + module + result/err
        }
        if (reloaded > 0) {
            m_warningText = "HotReload: " + std::to_string(reloaded)
                + " scene(s) reloaded";
            m_warningFrames = 120;
        }
        return true;
    }

    // -- Reload sequence --------------------------------------------------
    m_scriptState = ScriptState::RELOADING;
    DEBUG_INFO(SubSys::Dbg, ErrCode::Ok, "HotReload: starting script reload...");

    // Step 1: Cancel all active operations
    cancelAllActiveOps(m_L);

    // Cancellations are synchronous →→ no sleep needed

    // Step 2: Close the KAG coroutine if running
    if (pushGameState(m_L)) {
        lua_getfield(m_L, -1, "co");
        if (lua_isthread(m_L, -1)) {
            // Set stop_flag so scheduler won't resume
            lua_pushboolean(m_L, 1);
            lua_setfield(m_L, -3, "stop_flag");
            // Try to close the thread
            lua_getglobal(m_L, "coroutine");
            if (lua_istable(m_L, -1)) {
                lua_getfield(m_L, -1, "close");
                if (lua_isfunction(m_L, -1)) {
                    lua_pushvalue(m_L, -3);  // push co
                    if (lua_pcall(m_L, 1, 0, 0) != LUA_OK) {
                        lua_pop(m_L, 1);  // co may already be dead
                    }
                } else { lua_pop(m_L, 1); }
            }
            lua_pop(m_L, 1);  // pop coroutine table
        }
        lua_pop(m_L, 2);  // pop co (or nil), ctx -- NO, need ctx still
    }

    // Step 3: Reset GameState (sf/f preserved)
    if (pushGameState(m_L)) {
        lua_newtable(m_L); lua_setfield(m_L, -2, "call_stack");
        lua_newtable(m_L); lua_setfield(m_L, -2, "tokens");
        lua_pushinteger(m_L, 1); lua_setfield(m_L, -2, "token_index");
        lua_pushnil(m_L); lua_setfield(m_L, -2, "co");
        lua_newtable(m_L); lua_setfield(m_L, -2, "tf");
        lua_newtable(m_L); lua_setfield(m_L, -2, "layers");
        lua_newtable(m_L); lua_setfield(m_L, -2, "backlog");
        lua_newtable(m_L); lua_setfield(m_L, -2, "active_operations");
        lua_pushboolean(m_L, 0); lua_setfield(m_L, -2, "stop_flag");
        lua_pop(m_L, 1);

        DEBUG_INFO(SubSys::Dbg, ErrCode::Ok,
                   "HotReload: GameState reset complete (sf/f preserved).");
    }

    // Step 4: Reload script modules
    lua_getglobal(m_L, "package");
    if (lua_istable(m_L, -1)) {
        lua_getfield(m_L, -1, "loaded");
        if (lua_istable(m_L, -1)) {
            lua_pushnil(m_L);
            lua_setfield(m_L, -2, "kag");
        }
        lua_pop(m_L, 1);
    }
    lua_pop(m_L, 1);

    lua_getglobal(m_L, "require");
    lua_pushstring(m_L, "kag");
    if (lua_pcall(m_L, 1, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        fprintf(stderr, "[HotReload] kag reload failed: %s\n",
                err ? err : "(unknown error)");
        lua_pop(m_L, 1);
        // Surface the failure instead of pretending the reload succeeded.
        m_warningFrames = 120;
        m_warningText = "KAG reload FAILED";
        m_scriptState = ScriptState::IDLE;
        DEBUG_ERROR(SubSys::Dbg, ErrCode::Script_LoadFailed,
                    "HotReload: script reload failed after state reset.");
        return false;
    }

    // Step 5: Show warning overlay for ~2 seconds
    m_warningFrames = 120;
    m_warningText = "SCRIPTS RELOADED -- Changes applied.";
    m_scriptState = ScriptState::IDLE;

    DEBUG_INFO(SubSys::Dbg, ErrCode::Ok, "HotReload: reload complete.");
    return true;
}

void HotReload::showWarningOverlay(const std::string& message) {
    bgfx::dbgTextClear();
    uint16_t posY = 10;
    bgfx::dbgTextPrintf(0, posY++, 0x4E,
                        "  +----------------------------------------+");
    bgfx::dbgTextPrintf(0, posY++, 0x4E,
                        "  |                                        |");
    bgfx::dbgTextPrintf(0, posY++, 0x0E,
                        "  |  %-36s  |", message.c_str());
    bgfx::dbgTextPrintf(0, posY++, 0x4E,
                        "  |                                        |");
    bgfx::dbgTextPrintf(0, posY++, 0x4E,
                        "  +----------------------------------------+");
    bgfx::dbgTextPrintf(0, posY++, 0x0A,
                        "     [F5] Force Reload  |  [F6] Debug Toggle");
}

} // namespace Caesura
