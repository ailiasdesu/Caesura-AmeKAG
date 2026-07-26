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
    RpcGetDebugStateRequest>;

struct RpcRequest {
    RpcRequestPayload payload;
};

enum class RpcReplyStatus {
    Ok,
    InvalidRequest,
    Unavailable,
    Failed,
};

struct RpcStatusResult {
    bool luaReady = false;
};

struct RpcEvaluateResult {
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
    RpcDebugStateResult>;

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
