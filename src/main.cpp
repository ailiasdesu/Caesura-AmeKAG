extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include "render/BgfxRenderDevice.h"
#include "di/api/ITextureBudget.h"
#include "job/api/IJobSystem.h"
#include "render/api/IMeshRenderer.h"
#include "audio/SoLoudAudioEngine.h"
#include "platform/SDL3PlatformBackend.h"
#include "minigame/BgfxMiniGameBackend.h"
#include "live2d/api/IAnimationBackend.h"
#include "script/vm/LuaManager.h"
#include "entry/Engine.h"
#include "debug/DebugProtocol.h"
#include "rpc/EditorServer.h"
#include <nlohmann_json.hpp>
#include "rpc/RpcServer.h"
#include "rpc/api/IRpcDispatcher.h"
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

namespace Caesura {
std::string discoverStartupScriptDir();
void configureStartupLuaPath(lua_State* L, const std::string& scriptDir);
void applyDevModeToTextureManager(lua_State* L);
void validateCarcOnStartup(lua_State* L);
}

namespace {

Caesura::RpcReply rpcError(Caesura::RpcReplyStatus status,
                           const char* code,
                           const char* message) {
    return {status, code, message, {}};
}

// Asset paths for RPC validation are restricted to the repo's asset
// directories: relative, no "..", no absolute paths, no embedded quotes.
bool isSafeAssetPath(const std::string& path) {
    if (path.empty() || path.size() > 256) return false;
    if (path.find('"') != std::string::npos) return false;
    if (path.find("..") != std::string::npos) return false;
    if (path.front() == '/' || path.front() == '\\') return false;
    if (path.size() > 1 && path[1] == ':') return false;
    return path.rfind("assets/", 0) == 0 || path.rfind("demo/assets/", 0) == 0;
}

Caesura::RpcReply rpcOk() {
    return {Caesura::RpcReplyStatus::Ok, {}, {}, {}};
}

class EngineRpcDispatcher final : public Caesura::IRpcDispatcher {
public:
    explicit EngineRpcDispatcher(Caesura::Engine& engine)
        : m_engine(engine), m_ownerThread(std::this_thread::get_id()) {}

    ~EngineRpcDispatcher() override { close(); }

    Caesura::RpcReply dispatch(const Caesura::RpcRequest& request) override {
        if (std::this_thread::get_id() == m_ownerThread) {
            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                if (!m_accepting) return unavailable();
            }
            return executeSafely(request);
        }

        auto pending = std::make_shared<Pending>(request);
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (!m_accepting) return unavailable();
            m_pending.push_back(pending);
        }

        std::unique_lock<std::mutex> lock(pending->mutex);
        // Bounded wait: a stalled engine main thread (e.g. a GPU/driver
        // present hang) must not let HTTP worker threads pile up until the
        // accept backlog exhausts (observed as connection-refused on the IDE).
        // Default 5s; override with CAESURA_RPC_DISPATCH_TIMEOUT_MS.
        if (!pending->ready.wait_for(
                lock, std::chrono::milliseconds(dispatchTimeoutMs()),
                [&pending] { return pending->completed; })) {
            return rpcError(Caesura::RpcReplyStatus::Busy, "engine_busy",
                            "Engine main thread did not service the request in time");
        }
        return pending->reply;
    }

    void pump() {
        if (const char* stall = std::getenv("CAESURA_TEST_STALL_MS")) {
            // Diagnostic-only hook for end-to-end stall verification:
            // simulates a stalled engine main thread so the bounded dispatch
            // wait can be exercised without GPU/hardware involvement.
            char* end = nullptr;
            const long v = std::strtol(stall, &end, 10);
            if (end && *end == '\0' && v > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(v));
            }
        }
        std::deque<std::shared_ptr<Pending>> batch;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            batch.swap(m_pending);
        }
        for (const auto& pending : batch) {
            complete(pending, executeSafely(pending->request));
        }
        pumpManagedRuns();
    }

    void close() {
        std::deque<std::shared_ptr<Pending>> cancelled;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (!m_accepting) return;
            m_accepting = false;
            cancelled.swap(m_pending);
        }
        for (const auto& pending : cancelled) {
            complete(pending, unavailable());
        }
        abortManagedRuns();
    }

private:
    struct Pending {
        explicit Pending(const Caesura::RpcRequest& value) : request(value) {}

        Caesura::RpcRequest request;
        std::mutex mutex;
        std::condition_variable ready;
        bool completed = false;
        Caesura::RpcReply reply;
    };

    static Caesura::RpcReply unavailable() {
        return rpcError(Caesura::RpcReplyStatus::Unavailable,
                        "dispatcher_closed",
                        "Engine RPC dispatcher is closed");
    }

    // Bounded dispatch-wait budget. Default 5000ms; override with
    // CAESURA_RPC_DISPATCH_TIMEOUT_MS (diagnostics / slow-environment tuning).
    static long dispatchTimeoutMs() {
        const char* env = std::getenv("CAESURA_RPC_DISPATCH_TIMEOUT_MS");
        if (env) {
            char* end = nullptr;
            const long v = std::strtol(env, &end, 10);
            if (end && *end == '\0' && v > 0) return v;
        }
        return 5000L;
    }

    static void complete(const std::shared_ptr<Pending>& pending,
                         Caesura::RpcReply reply) {
        {
            std::lock_guard<std::mutex> lock(pending->mutex);
            if (pending->completed) return;
            pending->reply = std::move(reply);
            pending->completed = true;
        }
        pending->ready.notify_one();
    }

    Caesura::RpcReply executeSafely(const Caesura::RpcRequest& request) {
        try {
            return execute(request);
        } catch (const std::exception& error) {
            return {Caesura::RpcReplyStatus::Failed,
                    "owner_dispatch_exception", error.what(), {}};
        } catch (...) {
            return rpcError(Caesura::RpcReplyStatus::Failed,
                            "owner_dispatch_exception",
                            "Owner dispatcher threw an unknown exception");
        }
    }

    Caesura::RpcReply execute(const Caesura::RpcRequest& request) {
        return std::visit([this](const auto& operation) -> Caesura::RpcReply {
            using Operation = std::decay_t<decltype(operation)>;

            if constexpr (std::is_same_v<Operation, Caesura::RpcStatusRequest>) {
                Caesura::RpcReply reply = rpcOk();
                reply.payload = Caesura::RpcStatusResult{
                    m_engine.lua().state() != nullptr};
                return reply;
            } else if constexpr (std::is_same_v<Operation, Caesura::RpcRunScriptRequest>) {
                return startManagedRun(operation.script);
            } else if constexpr (std::is_same_v<Operation, Caesura::RpcEvaluateRequest>) {
                return evaluate(operation.code);
            } else if constexpr (std::is_same_v<Operation, Caesura::RpcKagDebugRequest>) {
                return kagDebugAction(operation);
            } else if constexpr (std::is_same_v<Operation, Caesura::RpcStopRequest>) {
                abortManagedRuns();
                m_engine.quit();
                return rpcOk();
            } else if constexpr (std::is_same_v<Operation, Caesura::RpcGetStateRequest>) {
                lua_State* L = m_engine.lua().state();
                if (!L) return rpcError(Caesura::RpcReplyStatus::Unavailable,
                                        "lua_unavailable", "Lua VM is unavailable");
                // Snapshot the runner ctx through Lua (kag_runner.get_ctx
                // is the authoritative game state the debugger also uses).
                const int stackTop = lua_gettop(L);
                const char* code =
                    "local ctx = require('kag_runner').get_ctx(); "
                    "if not ctx then return '{}' end; "
                    "local ok, layers = pcall(function() "
                    "  return require('layers').count() end); "
                    "local i18n = require('i18n'); "
                    "local bl = type(ctx.backlog) == 'table' and #ctx.backlog or 0; "
                    "local tok = ctx.tokens and ctx.tokens[ctx.token_index]; "
                    "local cur = ''; "
                    "if type(tok) == 'table' then "
                    "  if tok.type == 'command' then cur = '[' .. tostring(tok.cmd or '') .. ']' "
                    "  elseif tok.type == 'label' then cur = '*' .. tostring(tok.name or '') "
                    "  elseif tok.type == 'text' then cur = 'text' "
                    "  else cur = tostring(tok.type or '') end "
                    "end; "
                    "return string.format('{\"scene\":%q,\"token_index\":%d,"
                    "\"nvl_mode\":%s,\"language\":%q,\"backlog_count\":%d,"
                    "\"layer_count\":%d,\"current_cmd\":%q}', "
                    "tostring(ctx.current_scene or ctx.currentScene or ''), "
                    "tonumber(ctx.token_index) or 0, "
                    "ctx.nvl_mode == true and 'true' or 'false', "
                    "tostring(i18n and i18n.current or ''), bl, "
                    "ok and (tonumber(layers) or 0) or 0, cur)";
                Caesura::RpcStateResult state;
                if (luaL_loadstring(L, code) == LUA_OK
                    && lua_pcall(L, 0, 1, 0) == LUA_OK
                    && lua_isstring(L, -1)) {
                    // The snippet returns a JSON object literal.
                    try {
                        auto j = nlohmann::json::parse(lua_tostring(L, -1));
                        state.scene = j.value("scene", std::string());
                        state.tokenIndex = j.value("token_index", 0);
                        state.nvlMode = j.value("nvl_mode", false);
                        state.language = j.value("language", std::string());
                        state.backlogCount = j.value("backlog_count", 0);
                        state.layerCount = j.value("layer_count", 0);
                        state.currentCmd = j.value("current_cmd", std::string());
                    } catch (const std::exception&) {
                        state.scene = lua_tostring(L, -1);
                    }
                }
                lua_settop(L, stackTop);
                Caesura::RpcReply reply = rpcOk();
                reply.payload = std::move(state);
                return reply;
            } else if constexpr (std::is_same_v<Operation, Caesura::RpcSmaValidateRequest>) {
                if (!isSafeAssetPath(operation.path)) {
                    return rpcError(Caesura::RpcReplyStatus::InvalidRequest,
                                    "unsafe_path",
                                    "Path must be relative under assets/ or demo/assets/");
                }
                lua_State* L = m_engine.lua().state();
                if (!L) return rpcError(Caesura::RpcReplyStatus::Unavailable,
                                        "lua_unavailable", "Lua VM is unavailable");
                const int stackTop = lua_gettop(L);
                // Run the shared checker (kag.sma_check) inside the engine
                // Lua state: same module the runtime loader uses, so the
                // panel and the game agree on what is valid.
                std::string path = operation.path;
                for (char& ch : path) {
                    if (ch == '\\') ch = '/';
                }
                const std::string code =
                    "local ok, checker = pcall(require, 'kag.sma_check')\n"
                    "if not ok or not checker then return '{\"ok\":false,\"errors\":[\"sma_check unavailable\"],\"meta\":{}}' end\n"
                    "local function jl(list) local parts = {} for _, e in ipairs(list) do parts[#parts + 1] = string.format('%q', tostring(e)) end return '[' .. table.concat(parts, ',') .. ']' end\n"
                    "local function jm(meta) local anims = {} for _, a in ipairs(meta.anims or {}) do anims[#anims + 1] = string.format('%q', tostring(a)) end "
                    "local bt = {} for _, b in ipairs(meta.boneTree or {}) do bt[#bt + 1] = string.format('{\"id\":%d,\"parent\":%d,\"pivot\":[%s,%s]}', b.id, b.parent or -1, tostring(b.pivot and b.pivot[1] or 0), tostring(b.pivot and b.pivot[2] or 0)) end "
                    "local ad = {} for _, d in ipairs(meta.animDetails or {}) do ad[#ad + 1] = string.format('{\"name\":%q,\"duration\":%s,\"tracks\":%s}', tostring(d.name), d.duration or 0, jl(d.tracks or {})) end "
                    "return string.format('{\"bones\":%d,\"anims\":[%s],\"parts\":%d,\"verts\":%d,\"tris\":%d,\"boneTree\":[%s],\"animDetails\":[%s]}', "
                    "meta.bones or 0, table.concat(anims, ','), meta.parts or 0, meta.verts or 0, meta.tris or 0, table.concat(bt, ','), table.concat(ad, ',')) end\n"
                    "local res = checker.validate_file(\"" + path + "\")\n"
                    "return string.format('{\"ok\":%s,\"errors\":%s,\"meta\":%s}', "
                    "res.ok and 'true' or 'false', jl(res.errors or {}), jm(res.meta or {}))";
                Caesura::RpcSmaValidateResult result;
                if (luaL_loadstring(L, code.c_str()) == LUA_OK
                    && lua_pcall(L, 0, 1, 0) == LUA_OK
                    && lua_isstring(L, -1)) {
                    try {
                        auto j = nlohmann::json::parse(lua_tostring(L, -1));
                        result.ok = j.value("ok", false);
                        for (const auto& e : j.value("errors", std::vector<std::string>{})) {
                            result.errors.push_back(e);
                        }
                        result.meta = j.value("meta", nlohmann::json::object()).dump();
                    } catch (const std::exception&) {
                        result.ok = false;
                        result.errors.push_back("lua_result_parse_failed");
                    }
                } else {
                    result.ok = false;
                    result.errors.push_back("lua_exec_failed");
                }
                lua_settop(L, stackTop);
                Caesura::RpcReply reply = rpcOk();
                reply.payload = std::move(result);
                return reply;
            } else if constexpr (std::is_same_v<Operation, Caesura::RpcPickRequest>) {
                // IDE preview-frame pick: hit-test the Lua layer tree via
                // the shared layers module (same code the game uses).
                lua_State* L = m_engine.lua().state();
                if (!L) return rpcError(Caesura::RpcReplyStatus::Unavailable,
                                        "lua_unavailable", "Lua VM is unavailable");
                const int stackTop = lua_gettop(L);
                const std::string code =
                    "local ok, layers = pcall(require, 'layers')\n"
                    "if not ok or not layers or not layers.pick then return '[]' end\n"
                    "local hits = layers.pick(" + std::to_string(operation.x)
                    + ", " + std::to_string(operation.y) + ")\n"
                    "local parts = {}\n"
                    "for _, h in ipairs(hits) do\n"
                    "  parts[#parts + 1] = string.format('{\"id\":%q,\"name\":%q,\"z\":%d,\"depth\":%d,\"opacity\":%d,\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d}', "
                    "tostring(h.id), tostring(h.name), h.z or 0, h.depth or 0, h.opacity or 255, "
                    "math.floor(h.x or 0), math.floor(h.y or 0), math.floor(h.w or 0), math.floor(h.h or 0))\n"
                    "end\n"
                    "return '[' .. table.concat(parts, ',') .. ']'";
                Caesura::RpcPickResult pick;
                if (luaL_loadstring(L, code.c_str()) == LUA_OK
                    && lua_pcall(L, 0, 1, 0) == LUA_OK
                    && lua_isstring(L, -1)) {
                    pick.hits = lua_tostring(L, -1);
                } else {
                    pick.hits = "[]";
                }
                lua_settop(L, stackTop);
                Caesura::RpcReply reply = rpcOk();
                reply.payload = std::move(pick);
                return reply;
            } else if constexpr (std::is_same_v<Operation, Caesura::RpcSmaSaveRequest>) {
                // Editor save-back: validate the JSON text with the shared
                // checker first; only write to the disk on success. Path is
                // restricted by isSafeAssetPath (assets/ or demo/assets/).
                if (!isSafeAssetPath(operation.path)) {
                    return rpcError(Caesura::RpcReplyStatus::InvalidRequest,
                                    "unsafe_path",
                                    "Path must be relative under assets/ or demo/assets/");
                }
                lua_State* L = m_engine.lua().state();
                Caesura::RpcSmaSaveResult saveRes;
                if (!L) {
                    saveRes.ok = false;
                    saveRes.errors.push_back("lua_unavailable");
                } else {
                    const int stackTop = lua_gettop(L);
                    // Build a Lua literal for the content (escape backslash
                    // and quotes for the embedded string literal).
                    std::string lit = operation.content;
                    std::string esc;
                    esc.reserve(lit.size() + 16);
                    for (char ch : lit) {
                        if (ch == '\\') esc += "\\\\";
                        else if (ch == '"') esc += "\\\"";
                        else if (ch == '\n') esc += "\\n";
                        else if (ch == '\r') esc += "\\r";
                        else if (ch == '\t') esc += "\\t";
                        else esc += ch;
                    }
                    const std::string code =
                        "local ok, checker = pcall(require, 'kag.sma_check')\n"
                        "if not ok or not checker then return '{\"ok\":false,\"errors\":[\"sma_check unavailable\"]}' end\n"
                        "local res = checker.validate_text(\"" + esc + "\")\n"
                        "local parts = {} for _, e in ipairs(res.errors or {}) do parts[#parts + 1] = string.format('%q', tostring(e)) end\n"
                        "return string.format('{\"ok\":%s,\"errors\":[%s]}', res.ok and 'true' or 'false', table.concat(parts, ','))";
                    if (luaL_loadstring(L, code.c_str()) == LUA_OK
                        && lua_pcall(L, 0, 1, 0) == LUA_OK
                        && lua_isstring(L, -1)) {
                        try {
                            auto j = nlohmann::json::parse(lua_tostring(L, -1));
                            saveRes.ok = j.value("ok", false);
                            for (const auto& e2 : j.value("errors", std::vector<std::string>{})) {
                                saveRes.errors.push_back(e2);
                            }
                        } catch (...) {
                            saveRes.ok = false;
                            saveRes.errors.push_back("lua_result_parse_failed");
                        }
                    } else {
                        saveRes.ok = false;
                        saveRes.errors.push_back("lua_exec_failed");
                    }
                    lua_settop(L, stackTop);
                }
                if (saveRes.ok) {
                    std::string writePath = operation.path;
                    for (char& ch : writePath) {
                        if (ch == '/') ch = '\\';
                    }
                    std::ofstream out(writePath, std::ios::binary);
                    if (!out) {
                        saveRes.ok = false;
                        saveRes.errors.push_back("cannot open file for writing");
                    } else {
                        out.write(operation.content.data(),
                                  static_cast<std::streamsize>(operation.content.size()));
                        out.close();
                        if (!out) {
                            saveRes.ok = false;
                            saveRes.errors.push_back("write failed");
                        }
                    }
                }
                Caesura::RpcReply reply = rpcOk();
                reply.payload = std::move(saveRes);
                return reply;
            } else if constexpr (std::is_same_v<Operation, Caesura::RpcStatsRequest>) {
                Caesura::RpcStatsResult stats;
                stats.textureBudgetMB =
                    static_cast<int>(m_engine.textureBudget().getBudgetMB());
                stats.textureTier = m_engine.textureBudget().getTier();
                stats.textureTierName = m_engine.textureBudget().getTierName();
                stats.meshCount = m_engine.meshRenderer().meshCount();
                stats.jobWorkers = m_engine.jobSystem().workerCount();
                stats.jobPending = m_engine.jobSystem().pendingJobs();
                if (lua_State* L = m_engine.lua().state()) {
                    stats.luaKb = static_cast<int>(lua_gc(L, LUA_GCCOUNT, 0));
                }
                Caesura::RpcReply reply = rpcOk();
                reply.payload = std::move(stats);
                return reply;
            } else if constexpr (
                std::is_same_v<Operation, Caesura::RpcCaptureFrameRequest>) {
                std::string frame = m_engine.captureFrameForRpc(
                    operation.width, operation.height);
                if (frame.empty()) {
                    return rpcError(Caesura::RpcReplyStatus::Failed,
                                    "capture_failed", "Frame capture failed");
                }
                Caesura::RpcReply reply = rpcOk();
                reply.payload = Caesura::RpcFrameResult{std::move(frame)};
                return reply;
            } else if constexpr (
                std::is_same_v<Operation, Caesura::RpcReloadScriptsRequest>) {
                if (!m_engine.reloadScriptsNow()) {
                    return rpcError(Caesura::RpcReplyStatus::Failed,
                                    "reload_rejected",
                                    "Reload rejected (Lua paused or script reload failed)");
                }
                return rpcOk();
            } else if constexpr (
                std::is_same_v<Operation, Caesura::RpcLoadAnimationRequest>) {
                if (operation.modelPath.empty()) {
                    return rpcError(Caesura::RpcReplyStatus::InvalidRequest,
                                    "invalid_model_path", "Model path is empty");
                }
                if (!std::isfinite(operation.x) || !std::isfinite(operation.y) ||
                    !std::isfinite(operation.scale) || operation.scale <= 0.0f) {
                    return rpcError(Caesura::RpcReplyStatus::InvalidRequest,
                                    "invalid_animation_transform",
                                    "Animation transform must be finite and scale must be positive");
                }
                const std::string name = std::filesystem::path(
                    operation.modelPath).stem().string();
                const int modelId = m_engine.animation().loadModel(
                    operation.modelPath, name);
                if (modelId <= 0) {
                    return rpcError(Caesura::RpcReplyStatus::Failed,
                                    "animation_load_failed", "Animation load failed");
                }
                if (operation.show) {
                    try {
                        m_engine.animation().showModel(
                            modelId, operation.x, operation.y, operation.scale);
                    } catch (...) {
                        m_engine.animation().unloadModel(modelId);
                        throw;
                    }
                }
                Caesura::RpcReply reply = rpcOk();
                reply.payload = Caesura::RpcAnimationResult{modelId, name};
                return reply;
            } else {
                return executeDebug(operation);
            }
        }, request.payload);
    }

    template <typename Operation>
    Caesura::RpcReply executeDebug(const Operation& operation) {
        Caesura::DebugProtocol* protocol = m_engine.debugProtocol();

        if constexpr (std::is_same_v<Operation, Caesura::RpcGetDebugStateRequest>) {
            Caesura::RpcDebugStateResult result;
            if (protocol) {
                switch (protocol->runState()) {
                    case Caesura::DebugProtocol::RunState::Detached:
                        result.state = Caesura::RpcDebugRunState::Detached;
                        break;
                    case Caesura::DebugProtocol::RunState::Running:
                        result.state = Caesura::RpcDebugRunState::Running;
                        break;
                    case Caesura::DebugProtocol::RunState::Paused:
                        result.state = Caesura::RpcDebugRunState::Paused;
                        break;
                    case Caesura::DebugProtocol::RunState::ResumePending:
                        result.state = Caesura::RpcDebugRunState::ResumePending;
                        break;
                }
                result.source = protocol->currentSource();
                result.line = protocol->currentLine();
                result.pauseId = protocol->currentPauseId();
                result.nonYieldableHitCount = protocol->nonYieldableHitCount();
            }
            Caesura::RpcReply reply = rpcOk();
            reply.payload = std::move(result);
            return reply;
        }

        if (!protocol) {
            return rpcError(Caesura::RpcReplyStatus::Unavailable,
                            "debugger_disabled", "Debugger is not enabled");
        }

        if constexpr (std::is_same_v<Operation, Caesura::RpcSetBreakpointRequest>) {
            if (operation.source.empty() || operation.line <= 0) {
                return rpcError(Caesura::RpcReplyStatus::InvalidRequest,
                                "invalid_breakpoint", "Breakpoint source or line is invalid");
            }
            protocol->setBreakpoint(operation.source, operation.line);
            return rpcOk();
        } else if constexpr (
            std::is_same_v<Operation, Caesura::RpcRemoveBreakpointRequest>) {
            if (operation.source.empty() || operation.line <= 0) {
                return rpcError(Caesura::RpcReplyStatus::InvalidRequest,
                                "invalid_breakpoint", "Breakpoint source or line is invalid");
            }
            protocol->removeBreakpoint(operation.source, operation.line);
            return rpcOk();
        } else if constexpr (
            std::is_same_v<Operation, Caesura::RpcClearBreakpointsRequest>) {
            protocol->clearAllBreakpoints();
            return rpcOk();
        } else if constexpr (
            std::is_same_v<Operation, Caesura::RpcDebugResumeRequest>) {
            Caesura::DebugProtocol::Command command =
                Caesura::DebugProtocol::Command::Continue;
            switch (operation.mode) {
                case Caesura::RpcDebugResumeMode::Continue:
                    command = Caesura::DebugProtocol::Command::Continue;
                    break;
                case Caesura::RpcDebugResumeMode::StepInto:
                    command = Caesura::DebugProtocol::Command::StepInto;
                    break;
                case Caesura::RpcDebugResumeMode::StepOver:
                    command = Caesura::DebugProtocol::Command::StepOver;
                    break;
                case Caesura::RpcDebugResumeMode::StepOut:
                    command = Caesura::DebugProtocol::Command::StepOut;
                    break;
            }
            if (!protocol->commandSink()(operation.pauseId, command)) {
                return rpcError(Caesura::RpcReplyStatus::InvalidRequest,
                                "stale_pause", "Pause id is stale or no pause is active");
            }
            return rpcOk();
        } else if constexpr (
            std::is_same_v<Operation, Caesura::RpcInspectLocalRequest>) {
            if (!protocol->isDebugActive() || operation.frame < 0 ||
                operation.name.empty()) {
                return rpcError(Caesura::RpcReplyStatus::InvalidRequest,
                                "inspection_unavailable",
                                "Local inspection requires an active pause");
            }
            Caesura::RpcReply reply = rpcOk();
            reply.payload = Caesura::RpcInspectionResult{
                protocol->inspectLocal(operation.frame, operation.name)};
            return reply;
        } else if constexpr (
            std::is_same_v<Operation, Caesura::RpcInspectGlobalRequest>) {
            if (!protocol->isDebugActive() || operation.name.empty()) {
                return rpcError(Caesura::RpcReplyStatus::InvalidRequest,
                                "inspection_unavailable",
                                "Global inspection requires an active pause");
            }
            Caesura::RpcReply reply = rpcOk();
            reply.payload = Caesura::RpcInspectionResult{
                protocol->inspectGlobal(operation.name)};
            return reply;
        } else {
            return rpcError(Caesura::RpcReplyStatus::InvalidRequest,
                            "unknown_owner_command", "Unknown owner command");
        }
    }

    // -- Managed run/eval execution ------------------------------------

    struct ManagedRun {
        lua_State* co = nullptr;
        int slot = 0;
    };

    Caesura::RpcReply kagDebugAction(const Caesura::RpcKagDebugRequest& op) {
        // Drive the kag_debug.lua API through the Lua state. Each action
        // maps to a small Lua snippet so the editor never touches raw
        // Lua; results (inspect) come back as JSON text.
        lua_State* L = m_engine.lua().state();
        if (!L) {
            return rpcError(Caesura::RpcReplyStatus::Unavailable,
                            "lua_unavailable", "Lua VM is unavailable");
        }
        std::string code;
        if (op.action == "setBreakpoint") {
            if (op.scene.empty()
                || (op.cmd.empty() && op.line <= 0)) {
                return rpcError(Caesura::RpcReplyStatus::InvalidRequest,
                                "invalid_kag_breakpoint",
                                "scene plus cmd (string) or line (int) required");
            }
            code = "local kd = require('kag_debug'); "
                   "return tostring(kd.set_breakpoint("
                   + luaQuote(op.scene) + ", "
                   + (op.cmd.empty()
                       ? std::to_string(op.line)
                       : luaQuote(op.cmd))
                   + "))";
        } else if (op.action == "clearBreakpoints") {
            code = "local kd = require('kag_debug'); kd.clear_breakpoints("
                   + (op.scene.empty() ? "nil" : luaQuote(op.scene)) + "); "
                   + "return 'ok'";
        } else if (op.action == "continue") {
            code = "local kr = require('kag_runner'); "
                   "local ok = pcall(kr.debug_resume); "
                   "return ok and 'ok' or 'runner-not-ready'";
        } else if (op.action == "step") {
            code = "local kr = require('kag_runner'); "
                   "local ok = pcall(kr.debug_step); "
                   "return ok and 'ok' or 'runner-not-ready'";
        } else if (op.action == "reloadScene") {
            // Scene hot reload (editor workflow): re-parse the given (or
            // current) .ks through kag_runner.reload_scene -- preserves
            // game state and remaps the execution position.
            code = "local kr = require('kag_runner'); "
                   "local ok, r = kr.reload_scene("
                   + (op.scene.empty() ? "nil" : luaQuote(op.scene))
                   + "); "
                   "return ok and ('ok:' .. tostring(r)) "
                   "or ('error:' .. tostring(r))";
        } else if (op.action == "inspect") {
            code = "local kd = require('kag_debug'); "
                   "local ctx = require('kag_runner').get_ctx(); "
                   "if not ctx then return '{}' end; "
                   "return kd.serialize_json(ctx, "
                   + (op.scope.empty() ? "nil" : luaQuote(op.scope)) + ")";
        } else {
            return rpcError(Caesura::RpcReplyStatus::InvalidRequest,
                            "unknown_kag_debug_action",
                            ("unknown action: " + op.action).c_str());
        }

        const int top = lua_gettop(L);
        if (luaL_loadstring(L, code.c_str()) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            const std::string msg = err ? err : "compile error";
            lua_settop(L, top);
            return rpcError(Caesura::RpcReplyStatus::InvalidRequest,
                            "kag_debug_compile_error", msg.c_str());
        }
        const int callStatus = lua_pcall(L, 0, 1, 0);
        if (callStatus != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            const std::string msg = err ? err : "kag debug action failed";
            lua_settop(L, top);
            return rpcError(Caesura::RpcReplyStatus::Failed,
                            "kag_debug_error", msg.c_str());
        }
        std::string value = "nil";
        if (!lua_isnil(L, -1)) {
            size_t len = 0;
            const char* str = luaL_tolstring(L, -1, &len);
            if (str) value.assign(str, len);
        }
        lua_settop(L, top);
        Caesura::RpcReply reply = rpcOk();
        reply.payload = Caesura::RpcKagDebugResult{std::move(value)};
        return reply;
    }

    // Quote a string as a Lua literal (single-quoted with escapes).
    static std::string luaQuote(const std::string& s) {
        std::string out = "'";
        for (char c : s) {
            switch (c) {
            case '\\': out += "\\\\"; break;
            case '\'': out += "\\'"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default: out += c; break;
            }
        }
        out += "'";
        return out;
    }

    Caesura::RpcReply evaluate(const std::string& code) {
        lua_State* L = m_engine.lua().state();
        if (!L) {
            return rpcError(Caesura::RpcReplyStatus::Unavailable,
                            "lua_unavailable", "Lua VM is unavailable");
        }        const int top = lua_gettop(L);
        if (luaL_loadstring(L, code.c_str()) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            const std::string msg = err ? err : "compile error";
            lua_settop(L, top);
            return rpcError(Caesura::RpcReplyStatus::InvalidRequest,
                            "eval_compile_error", msg.c_str());
        }
        const int callStatus = lua_pcall(L, 0, 1, 0);
        if (callStatus != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            const std::string msg = (callStatus == LUA_YIELD)
                ? "eval cannot yield"
                : (err ? err : "evaluation failed");
            lua_settop(L, top);
            return rpcError(Caesura::RpcReplyStatus::Failed,
                            "eval_error", msg.c_str());
        }
        std::string value = "nil";
        if (!lua_isnil(L, -1)) {
            size_t len = 0;
            const char* str = luaL_tolstring(L, -1, &len);
            if (str) value.assign(str, len);
        }
        lua_settop(L, top);
        Caesura::RpcReply reply = rpcOk();
        reply.payload = Caesura::RpcEvaluateResult{std::move(value)};
        return reply;
    }

    Caesura::RpcReply startManagedRun(const std::string& script) {
        lua_State* L = m_engine.lua().state();
        if (!L) {
            return rpcError(Caesura::RpcReplyStatus::Unavailable,
                            "lua_unavailable", "Lua VM is unavailable");
        }
        if (script.empty()) {
            return rpcError(Caesura::RpcReplyStatus::InvalidRequest,
                            "empty_script", "Script must not be empty");
        }
        lua_State* co = lua_newthread(L);
        if (luaL_loadstring(co, script.c_str()) != LUA_OK) {
            const char* err = lua_tostring(co, -1);
            const std::string msg = err ? err : "compile error";
            lua_pop(co, 1);
            lua_pop(L, 1);  // drop the thread
            return rpcError(Caesura::RpcReplyStatus::InvalidRequest,
                            "run_compile_error", msg.c_str());
        }
        const int slot = 0x5100 + static_cast<int>(m_managedRuns.size());
        m_managedRuns.push_back(ManagedRun{co, slot});
        // Keep the thread alive across GC: registry holds a strong reference.
        lua_pushvalue(L, -1);
        lua_rawseti(L, LUA_REGISTRYINDEX, slot);
        lua_pop(L, 1);
        Caesura::RpcReply reply = rpcOk();
        reply.message = "started";
        return reply;
    }

    void pumpManagedRuns() {
        if (m_managedRuns.empty()) return;
        lua_State* L = m_engine.lua().state();
        if (!L) {
            m_managedRuns.clear();
            return;
        }
        for (auto it = m_managedRuns.begin(); it != m_managedRuns.end();) {
            int nresults = 0;
            const int status = lua_resume(it->co, L, 0, &nresults);
            if (status == LUA_YIELD) {
                ++it;
                continue;
            }
            if (status != LUA_OK) {
                const char* err = lua_tostring(it->co, -1);
                fprintf(stderr, "[RpcRun] script finished with error: %s\n",
                        err ? err : "unknown error");
            }
            lua_settop(it->co, 0);
            lua_pushnil(L);
            lua_rawseti(L, LUA_REGISTRYINDEX, it->slot);
            it = m_managedRuns.erase(it);
        }
    }

    void abortManagedRuns() {
        if (m_managedRuns.empty()) return;
        lua_State* L = m_engine.lua().state();
        if (L) {
            for (const auto& run : m_managedRuns) {
                lua_pushnil(L);
                lua_rawseti(L, LUA_REGISTRYINDEX, run.slot);
            }
        }
        m_managedRuns.clear();
    }

    Caesura::Engine& m_engine;
    std::thread::id m_ownerThread;
    std::mutex m_queueMutex;
    std::deque<std::shared_ptr<Pending>> m_pending;
    std::deque<ManagedRun> m_managedRuns;
    bool m_accepting = true;
};

void runStdioRpc(Caesura::Engine& engine) {
    auto dispatcher = std::make_shared<EngineRpcDispatcher>(engine);
    Caesura::RpcServer rpc;
    rpc.setDispatcher(dispatcher);

    std::atomic<bool> transportFinished{false};
    std::thread transport([&rpc, &transportFinished]() {
        rpc.run();
        transportFinished.store(true, std::memory_order_release);
    });

    engine.run([&]() {
        dispatcher->pump();
        if (transportFinished.load(std::memory_order_acquire)) engine.quit();
    });

    dispatcher->close();
    rpc.stop();
    rpc.setDispatcher({});
    if (transport.joinable()) transport.join();
    engine.shutdown();
}

bool runHttpEditor(Caesura::Engine& engine, const std::string& authToken) {
    auto dispatcher = std::make_shared<EngineRpcDispatcher>(engine);
    Caesura::EditorServer editor;
    editor.setDispatcher(dispatcher);
    if (!authToken.empty()) editor.setAuthToken(authToken);
    // Serve the bundled web-editor frontend (web-editor/dist). Resolve it
    // relative to the current directory, walking up a few levels so the
    // editor works whether launched from the repo root or a build dir
    // (e.g. build/Debug -> ../../web-editor/dist).
    {
        namespace fs = std::filesystem;
        std::string webRoot;
        fs::path probe = fs::current_path();
        for (int i = 0; i < 4 && webRoot.empty(); ++i) {
            auto candidate = probe / "web-editor" / "dist" / "index.html";
            if (fs::exists(candidate)) {
                webRoot = (probe / "web-editor" / "dist").string();
            }
            probe = probe.parent_path();
        }
        if (!webRoot.empty()) editor.setWebRoot(webRoot);
        else fprintf(stderr, "[EditorServer] web-editor/dist not found; serving API only\n");
    }
    if (!editor.start(9876)) {
        editor.setDispatcher({});
        dispatcher->close();
        engine.shutdown();
        return false;
    }

    engine.run([&]() { dispatcher->pump(); });

    dispatcher->close();
    editor.setDispatcher({});
    editor.stop();
    engine.shutdown();
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    fprintf(stderr, "[main] Starting Caesura (AmeKAG)...\n");

    // Resources resolve relative to the CWD; if the user launched the
    // engine from a build dir (or anywhere else), walk up until assets/
    // is found and chdir there so the game finds its data regardless of
    // the launch directory.
    {
        namespace fs = std::filesystem;
        fs::path probe = fs::current_path();
        for (int i = 0; i < 6; ++i) {
            if (fs::exists(probe / "assets") && fs::is_directory(probe / "assets")) {
                std::error_code ec;
                fs::current_path(probe, ec);
                if (!ec) {
                    fprintf(stderr, "[main] Working directory: %s\n", probe.string().c_str());
                }
                break;
            }
            probe = probe.parent_path();
        }
    }

    // -- Parse CLI flags -------------------------------------------------
    bool headless = false;
    bool editorMode = false;
    bool editorStdio = false;
    // Optional GPU backend override: --backend <opengl|vulkan|dx11|dx12|metal|webgpu>
    std::string renderBackend;
    // Optional deterministic frame limit: --frames N (GPU smoke runs; 0 = unlimited)
    uint32_t frameLimit = 0;
    // Optional demo/video export: --export-replay <replay.json> drives the
    // recorded input while each rendered frame is written as PNG into
    // --export-dir (default export_out). Bounded by --frames N.
    std::string exportReplayFile;
    std::string exportDir = "export_out";
    // Editor auth token comes from the environment, not argv: argv is
    // world-readable via /proc/<pid>/cmdline on Linux, so a CLI flag would
    // not protect against other local users. Set CAESURA_EDITOR_TOKEN to
    // require a bearer token on every HTTP editor request.
    const char* envToken = std::getenv("CAESURA_EDITOR_TOKEN");
    std::string editorToken = envToken ? envToken : "";
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--headless") {
            headless = true;
        } else if (arg == "--editor") {
            editorMode = true;
        } else if (arg == "--editor-stdio") {
            editorMode = true;
            editorStdio = true;
        } else if (arg == "--backend" && i + 1 < argc) {
            renderBackend = argv[++i];
        } else if (arg == "--frames" && i + 1 < argc) {
            char* end = nullptr;
            const long v = strtol(argv[++i], &end, 10);
            if (end && *end == '\0' && v > 0 && v <= 1000000L) {
                frameLimit = static_cast<uint32_t>(v);
            } else {
                fprintf(stderr, "Invalid --frames value: %s\n", argv[i]);
                return 1;
            }
        } else if (arg == "--export-replay" && i + 1 < argc) {
            exportReplayFile = argv[++i];
        } else if (arg == "--export-dir" && i + 1 < argc) {
            exportDir = argv[++i];
        }
    }

    // Demo/video export needs a real GPU window (bgfx readback): --headless
    // uses NullRenderDevice and cannot capture frames.
    if (!exportReplayFile.empty() && headless) {
        fprintf(stderr, "[main] --export-replay requires a GPU window;"
                        " ignoring --headless.\n");
        headless = false;
    }

    printf("============================================\n");
    printf("  Caesura (AmeKAG) v1.0.0\n");
    printf("  Cross-platform Visual Novel Engine\n");
    printf("  SDL3 + bgfx + SoLoud + Lua\n");
    if (headless) printf("  [HEADLESS MODE]\n");
    printf("============================================\n\n");

    Caesura::EngineConfig config;
    config.title      = "Caesura (AmeKAG)";
    config.width      = 1280;
    config.height     = 720;
    config.headless   = headless;
    config.editorMode = editorMode;
    config.enableDebugger = headless || editorMode;
    config.renderBackend  = renderBackend.empty() ? nullptr : renderBackend.c_str();
    config.frameLimit     = frameLimit;
    config.exportReplayFile = exportReplayFile;
    config.exportDir        = exportDir;

    // Create GPU-mode implementations here; Engine supplies safe defaults otherwise.
    if (!headless || editorMode) {
        config.platform = new Caesura::SDL3PlatformBackend();
        config.render   = new Caesura::BgfxRenderDevice();
        config.audio    = new Caesura::SoLoudAudioEngine();
        config.miniGame = new Caesura::BgfxMiniGameBackend();
    }

    Caesura::Engine engine(std::move(config));

    if (!engine.init()) {
        fprintf(stderr, "Failed to initialize engine.\n");
        return 1;
    }

    // -- Editor mode: hidden window + owner-thread RPC dispatcher ----------
    if (editorMode) {
        fprintf(stderr, editorStdio
            ? "[main] Editor mode: JSON-RPC on stdin/stdout (GPU enabled)\n"
            : "[main] Editor mode: HTTP editor on port 9876 (GPU enabled)\n");

        std::string scriptDir = Caesura::discoverStartupScriptDir();
        Caesura::configureStartupLuaPath(engine.lua().state(), scriptDir);
        engine.lua().loadScript((scriptDir + "config.lua").c_str());
        engine.lua().loadScript((scriptDir + "kag/init.lua").c_str());
        engine.lua().lockdownScriptEnv();

        const bool editorOk = editorStdio
            ? (runStdioRpc(engine), true)
            : runHttpEditor(engine, editorToken);
        if (!editorOk) return 1;
        printf("Caesura (AmeKAG) shut down cleanly.\n");
        return 0;
    }

    // -- Headless mode: stdin/stdout JSON-RPC -----------------------------
    if (headless) {
        fprintf(stderr, "[main] Headless mode: JSON-RPC on stdin/stdout\n");

        // Load minimal config for Lua VM
        std::string scriptDir = Caesura::discoverStartupScriptDir();
        Caesura::configureStartupLuaPath(engine.lua().state(), scriptDir);
        engine.lua().loadScript((scriptDir + "config.lua").c_str());
        engine.lua().loadScript((scriptDir + "kag/init.lua").c_str());
        engine.lua().lockdownScriptEnv();

        runStdioRpc(engine);
        printf("Caesura (AmeKAG) shut down cleanly.\n");
        return 0;
    }

    std::string scriptDir = Caesura::discoverStartupScriptDir();
    lua_State* L = engine.lua().state();
    Caesura::configureStartupLuaPath(L, scriptDir);

    // Load config first (backend selection happens here)
    engine.lua().loadScript((scriptDir + "config.lua").c_str());

    // [10.2.57] Apply dev mode to placeholder texture
    Caesura::applyDevModeToTextureManager(L);

    // Load KAG init (loads all Lua libraries)
    if (!engine.lua().loadScript((scriptDir + "kag/init.lua").c_str())) {
        fprintf(stderr, "Warning: Failed to load KAG init.\n");
    }

    // [10.2.30] CARC startup validation
    Caesura::validateCarcOnStartup(L);

    // One-time startup loads are separate budget windows from the per-frame
    // game loop: reset the instruction budget before parsing the entry scene.
    engine.lua().resetInstructionBudget();

    // Load main game logic (entry point from config) [10.2.30]
    std::string entryScript = "game_logic.lua";
    if (L) {
        lua_getglobal(L, "config");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "entry_script");
            if (lua_isstring(L, -1)) {
                entryScript = lua_tostring(L, -1);
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }

    if (!engine.lua().loadScript((scriptDir + entryScript).c_str())) {
        fprintf(stderr, "Warning: Failed to load game_logic.lua.\n");
        return 1;
    }

    // Push _CAESURA_CONFIG global for sandbox to read
    lua_getglobal(L, "config");  // config table loaded by config.lua
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "dev_mode");
        bool devMode = lua_toboolean(L, -1);
        lua_pop(L, 1);
        
        lua_newtable(L);
        lua_pushboolean(L, devMode ? 1 : 0);
        lua_setfield(L, -2, "dev_mode");
        lua_setglobal(L, "_CAESURA_CONFIG");
        printf("[main] _CAESURA_CONFIG.dev_mode = %s\n", devMode ? "true" : "false");
    }
    lua_pop(L, 1);


    // C3+W8: lockdown script env after ALL scripts are preloaded
    engine.lua().lockdownScriptEnv();

    // Demo/video export: activate replay playback before the main loop.
    // kag_runner.update() fires the recorded events (same on_click path);
    // Engine::run writes one PNG per frame into the export dir.
    if (!exportReplayFile.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(exportDir, ec);
        lua_State* exL = engine.lua().state();
        if (exL) {
            lua_getglobal(exL, "require");
            lua_pushstring(exL, "replay");
            if (lua_pcall(exL, 1, 1, 0) == LUA_OK && lua_istable(exL, -1)) {
                lua_getfield(exL, -1, "set_mode");
                if (lua_isfunction(exL, -1)) {
                    lua_pushvalue(exL, -2);  // self
                    lua_pushstring(exL, "playback");
                    lua_pushstring(exL, exportReplayFile.c_str());
                    if (lua_pcall(exL, 3, 0, 0) != LUA_OK) {
                        fprintf(stderr, "[main] replay set_mode failed: %s\n",
                                lua_tostring(exL, -1) ? lua_tostring(exL, -1)
                                                      : "unknown");
                    }
                }
            }
            lua_settop(exL, 0);
        }
        printf("[main] Export mode: replay %s -> %s (frames=%u)\n",
               exportReplayFile.c_str(), exportDir.c_str(), frameLimit);
    }

    engine.run();
    engine.shutdown();

    printf("Caesura (AmeKAG) shut down cleanly.\n");
    return 0;
}
