extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include "render/BgfxRenderDevice.h"
#include "audio/SoLoudAudioEngine.h"
#include "platform/SDL3PlatformBackend.h"
#include "minigame/BgfxMiniGameBackend.h"
#include "live2d/api/IAnimationBackend.h"
#include "script/vm/LuaManager.h"
#include "entry/Engine.h"
#include "debug/DebugProtocol.h"
#include "rpc/EditorServer.h"
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
        pending->ready.wait(lock, [&pending] { return pending->completed; });
        return pending->reply;
    }

    void pump() {
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
            } else if constexpr (std::is_same_v<Operation, Caesura::RpcStopRequest>) {
                abortManagedRuns();
                m_engine.quit();
                return rpcOk();
            } else if constexpr (std::is_same_v<Operation, Caesura::RpcGetStateRequest>) {
                lua_State* L = m_engine.lua().state();
                if (!L) return rpcError(Caesura::RpcReplyStatus::Unavailable,
                                        "lua_unavailable", "Lua VM is unavailable");
                const int stackTop = lua_gettop(L);
                lua_getglobal(L, "_KAG_SceneName");
                std::string scene;
                if (lua_isstring(L, -1)) scene = lua_tostring(L, -1);
                lua_settop(L, stackTop);
                Caesura::RpcReply reply = rpcOk();
                reply.payload = Caesura::RpcStateResult{std::move(scene)};
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
                                    "Reload is unavailable while Lua is paused");
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

    Caesura::RpcReply evaluate(const std::string& code) {
        lua_State* L = m_engine.lua().state();
        if (!L) {
            return rpcError(Caesura::RpcReplyStatus::Unavailable,
                            "lua_unavailable", "Lua VM is unavailable");
        }
        const int top = lua_gettop(L);
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

    // -- Parse CLI flags -------------------------------------------------
    bool headless = false;
    bool editorMode = false;
    bool editorStdio = false;
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
        }
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

    engine.run();
    engine.shutdown();

    printf("Caesura (AmeKAG) shut down cleanly.\n");
    return 0;
}
