// IRpcDispatcher - owner-thread command boundary for RPC transports.
#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace Caesura {

struct RpcStatusRequest {};

struct RpcRunScriptRequest {
    std::string script;
};

struct RpcStopRequest {};

struct RpcEvaluateRequest {
    std::string code;
};

struct RpcGetStateRequest {};

struct RpcCaptureFrameRequest {
    int width = 1280;
    int height = 720;
};

struct RpcReloadScriptsRequest {};

struct RpcLoadAnimationRequest {
    std::string modelPath;
    float x = 0.0f;
    float y = 0.0f;
    float scale = 1.0f;
    bool show = true;
};

struct RpcSetBreakpointRequest {
    std::string source;
    int line = 0;
};

struct RpcRemoveBreakpointRequest {
    std::string source;
    int line = 0;
};

struct RpcClearBreakpointsRequest {};

enum class RpcDebugResumeMode {
    Continue,
    StepInto,
    StepOver,
    StepOut,
};

struct RpcDebugResumeRequest {
    std::uint64_t pauseId = 0;
    RpcDebugResumeMode mode = RpcDebugResumeMode::Continue;
};

struct RpcInspectLocalRequest {
    int frame = 0;
    std::string name;
};

struct RpcInspectGlobalRequest {
    std::string name;
};

struct RpcGetDebugStateRequest {};

// KAG scene-level debugger (Neo-Genesis): breakpoints on scene+command
// or scene+line, continue/step, and variable-scope inspection. The Lua
// debugger (DebugProtocol) cannot see KAG tokens; these operations drive
// the kag_debug.lua API through the Lua state.
struct RpcKagDebugRequest {
    std::string action;   // setBreakpoint | clearBreakpoints | continue | step | inspect
    std::string scene;    // setBreakpoint / clearBreakpoints (exact scheduler scene path)
    std::string cmd;      // setBreakpoint: command name (mutually exclusive with line)
    int line = 0;         // setBreakpoint: 1-based token line
    std::string scope;    // inspect: f | sf | tf | mp | lf | all (default all)
};

using RpcRequestPayload = std::variant<
    RpcStatusRequest,
    RpcRunScriptRequest,
    RpcStopRequest,
    RpcEvaluateRequest,
    RpcGetStateRequest,
    RpcCaptureFrameRequest,
    RpcReloadScriptsRequest,
    RpcLoadAnimationRequest,
    RpcSetBreakpointRequest,
    RpcRemoveBreakpointRequest,
    RpcClearBreakpointsRequest,
    RpcDebugResumeRequest,
    RpcInspectLocalRequest,
    RpcInspectGlobalRequest,
    RpcGetDebugStateRequest,
    RpcKagDebugRequest>;

struct RpcRequest {
    RpcRequestPayload payload;
};

enum class RpcReplyStatus {
    Ok,
    InvalidRequest,
    Unavailable,
    Busy, // engine main thread did not service the request in time
    Failed,
};

struct RpcStatusResult {
    bool luaReady = false;
};

struct RpcEvaluateResult {
    std::string value;
};

// KAG scene debugger result: inspect returns a JSON object describing the
// requested variable scope(s); other actions return "ok".
struct RpcKagDebugResult {
    std::string value;
};

struct RpcStateResult {
    std::string scene;
};

struct RpcFrameResult {
    std::string base64;
};

struct RpcAnimationResult {
    int modelId = 0;
    std::string name;
};

struct RpcInspectionResult {
    std::string value;
};

enum class RpcDebugRunState {
    Detached,
    Running,
    Paused,
    ResumePending,
};

struct RpcDebugStateResult {
    RpcDebugRunState state = RpcDebugRunState::Detached;
    std::string source;
    int line = 0;
    std::uint64_t pauseId = 0;
    std::uint64_t nonYieldableHitCount = 0;
};

using RpcReplyPayload = std::variant<
    std::monostate,
    RpcStatusResult,
    RpcEvaluateResult,
    RpcStateResult,
    RpcFrameResult,
    RpcAnimationResult,
    RpcInspectionResult,
    RpcDebugStateResult,
    RpcKagDebugResult>;

struct RpcReply {
    RpcReplyStatus status = RpcReplyStatus::Unavailable;
    std::string code;
    std::string message;
    RpcReplyPayload payload;
};

class IRpcDispatcher {
public:
    virtual ~IRpcDispatcher() = default;

    virtual RpcReply dispatch(const RpcRequest& request) = 0;
};

} // namespace Caesura
