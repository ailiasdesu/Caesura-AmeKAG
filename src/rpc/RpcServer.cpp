// ===========================================================================
//  Caesura (AmeKAG) -- RpcServer implementation
//  stdin/stdout JSON-RPC — simplest possible protocol.
//  Each line is a complete JSON object, \n delimited.
// ===========================================================================

#include "RpcServer.h"

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <exception>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
namespace Caesura {

namespace {

void stripUtf8Bom(std::string& line) {
    if (line.size() >= 3 &&
        static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) {
        line.erase(0, 3);
    }
}

void trimCarriageReturn(std::string& line) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
}

class StdioLineReader {
public:
    StdioLineReader() {
#if defined(_WIN32)
        m_input = GetStdHandle(STD_INPUT_HANDLE);
#else
        if (::pipe(m_cancelPipe) == 0) {
            for (int fd : m_cancelPipe) {
                const int flags = ::fcntl(fd, F_GETFL, 0);
                if (flags >= 0) ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            }
        } else {
            m_cancelPipe[0] = -1;
            m_cancelPipe[1] = -1;
        }
#endif
    }

    ~StdioLineReader() {
        cancel();
#if !defined(_WIN32)
        if (m_cancelPipe[0] >= 0) ::close(m_cancelPipe[0]);
        if (m_cancelPipe[1] >= 0) ::close(m_cancelPipe[1]);
#endif
    }

    RpcLineReadResult readLine(std::string& line) {
        line.clear();
        for (;;) {
            if (m_cancelled.load(std::memory_order_acquire)) {
                return RpcLineReadResult::Cancelled;
            }

            const std::size_t newline = m_buffer.find('\n');
            if (newline != std::string::npos) {
                line.assign(m_buffer, 0, newline);
                m_buffer.erase(0, newline + 1);
                trimCarriageReturn(line);
                return RpcLineReadResult::Line;
            }

            if (m_endOfInput) {
                if (!m_buffer.empty()) {
                    line.swap(m_buffer);
                    trimCarriageReturn(line);
                    return RpcLineReadResult::Line;
                }
                return RpcLineReadResult::EndOfInput;
            }

            const RpcLineReadResult result = readMore();
            if (result != RpcLineReadResult::Line) {
                if (result == RpcLineReadResult::EndOfInput) {
                    m_endOfInput = true;
                    continue;
                }
                return result;
            }
        }
    }

    void cancel() {
        if (m_cancelled.exchange(true, std::memory_order_acq_rel)) return;

#if defined(_WIN32)
        std::unique_lock<std::mutex> lock(m_readMutex);
        while (m_readInProgress) {
            if (m_readerThread) CancelSynchronousIo(m_readerThread);
            if (m_input && m_input != INVALID_HANDLE_VALUE) {
                CancelIoEx(m_input, nullptr);
            }
            if (m_readInProgress) {
                m_readFinished.wait_for(lock, std::chrono::milliseconds(2));
            }
        }
#else
        if (m_cancelPipe[1] >= 0) {
            const char wake = 1;
            const ssize_t ignored = ::write(m_cancelPipe[1], &wake, sizeof(wake));
            (void)ignored;
        }
#endif
    }

private:
    RpcLineReadResult readMore() {
#if defined(_WIN32)
        if (!m_input || m_input == INVALID_HANDLE_VALUE) {
            return RpcLineReadResult::EndOfInput;
        }

        HANDLE readerThread = nullptr;
        if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                             GetCurrentProcess(), &readerThread, 0, FALSE,
                             DUPLICATE_SAME_ACCESS)) {
            return RpcLineReadResult::EndOfInput;
        }

        {
            std::lock_guard<std::mutex> lock(m_readMutex);
            if (m_cancelled.load(std::memory_order_acquire)) {
                CloseHandle(readerThread);
                return RpcLineReadResult::Cancelled;
            }
            m_readerThread = readerThread;
            m_readInProgress = true;
        }

        char bytes[4096];
        DWORD bytesRead = 0;
        const BOOL ok = ReadFile(
            m_input, bytes, static_cast<DWORD>(sizeof(bytes)), &bytesRead, nullptr);
        const DWORD error = ok ? ERROR_SUCCESS : GetLastError();

        {
            std::lock_guard<std::mutex> lock(m_readMutex);
            m_readInProgress = false;
            m_readerThread = nullptr;
        }
        CloseHandle(readerThread);
        m_readFinished.notify_all();

        if (m_cancelled.load(std::memory_order_acquire) ||
            (!ok && error == ERROR_OPERATION_ABORTED)) {
            return RpcLineReadResult::Cancelled;
        }
        if (!ok || bytesRead == 0) return RpcLineReadResult::EndOfInput;

        m_buffer.append(bytes, static_cast<std::size_t>(bytesRead));
        return RpcLineReadResult::Line;
#else
        if (m_cancelPipe[0] < 0) return RpcLineReadResult::EndOfInput;

        pollfd descriptors[2] = {
            {STDIN_FILENO, POLLIN | POLLHUP | POLLERR, 0},
            {m_cancelPipe[0], POLLIN | POLLHUP | POLLERR, 0},
        };

        int ready = 0;
        do {
            ready = ::poll(descriptors, 2, -1);
        } while (ready < 0 && errno == EINTR &&
                 !m_cancelled.load(std::memory_order_acquire));

        if (m_cancelled.load(std::memory_order_acquire) ||
            (descriptors[1].revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
            return RpcLineReadResult::Cancelled;
        }
        if (ready < 0) return RpcLineReadResult::EndOfInput;

        char bytes[4096];
        ssize_t bytesRead = 0;
        do {
            bytesRead = ::read(STDIN_FILENO, bytes, sizeof(bytes));
        } while (bytesRead < 0 && errno == EINTR);

        if (bytesRead < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return RpcLineReadResult::Line;
        }
        if (bytesRead <= 0) return RpcLineReadResult::EndOfInput;

        m_buffer.append(bytes, static_cast<std::size_t>(bytesRead));
        return RpcLineReadResult::Line;
#endif
    }

    std::atomic<bool> m_cancelled{false};
    std::string m_buffer;
    bool m_endOfInput = false;

#if defined(_WIN32)
    HANDLE m_input = INVALID_HANDLE_VALUE;
    std::mutex m_readMutex;
    std::condition_variable m_readFinished;
    HANDLE m_readerThread = nullptr;
    bool m_readInProgress = false;
#else
    int m_cancelPipe[2] = {-1, -1};
#endif
};

RpcLineSource makeStdioLineSource() {
    auto reader = std::make_shared<StdioLineReader>();
    return {
        [reader](std::string& line) { return reader->readLine(line); },
        [reader]() { reader->cancel(); },
    };
}

} // namespace

// =========================================================================
// Lifecycle
// =========================================================================

RpcServer::RpcServer()
    : RpcServer(makeStdioLineSource()) {}

RpcServer::RpcServer(RpcLineSource lineSource)
    : m_lineSource(std::move(lineSource)) {}

RpcServer::~RpcServer() {
    stop();
}

void RpcServer::run() {
    if (m_stopRequested.load(std::memory_order_acquire)) return;
    if (!m_lineSource.readLine) {
        fprintf(stderr, "[RpcServer] No input line source.\n");
        return;
    }

    m_running.store(true, std::memory_order_release);
    if (m_stopRequested.load(std::memory_order_acquire)) {
        m_running.store(false, std::memory_order_release);
        return;
    }

    std::string line;

    fprintf(stderr, "[RpcServer] JSON-RPC ready on stdin/stdout\n");

    while (!m_stopRequested.load(std::memory_order_acquire)) {
        const RpcLineReadResult readResult = m_lineSource.readLine(line);
        if (readResult != RpcLineReadResult::Line) break;
        stripUtf8Bom(line);
        if (line.empty()) continue;
        if (line[0] != '{') continue;  // skip non-JSON

        std::string response = processRequestLine(line);
        if (!response.empty()) {
            writeLine(response);
        }
    }

    m_running.store(false, std::memory_order_release);
    fprintf(stderr, "[RpcServer] Shutdown.\n");
}

void RpcServer::stop() {
    m_stopRequested.store(true, std::memory_order_release);
    m_running.store(false, std::memory_order_release);
    if (m_lineSource.cancel) m_lineSource.cancel();
}

void RpcServer::setDispatcher(std::shared_ptr<IRpcDispatcher> dispatcher) {
    std::lock_guard<std::mutex> lock(m_dispatcherMutex);
    m_dispatcher = std::move(dispatcher);
}

// =========================================================================
// Log buffer (thread-safe output)
// =========================================================================

void RpcServer::pushLog(const std::string& level, const std::string& message) {
    std::ostringstream json;
    json << "{\"event\":\"log\",\"level\":\"" << jsonEscape(level)
         << "\",\"message\":\"" << jsonEscape(message) << "\"}";
    writeLine(json.str());
}

void RpcServer::writeLine(const std::string& json) {
    std::lock_guard<std::mutex> lock(m_writeMutex);
    std::cout << json << std::endl;
    std::cout.flush();
}

// =========================================================================
// Request dispatcher
// =========================================================================

static int parseId(const std::string& json) {
    // Quick and dirty: find "id":N
    size_t pos = json.find("\"id\":");
    if (pos == std::string::npos) return 0;
    pos += 5;
    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    // Read number
    int val = 0;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
        val = val * 10 + (json[pos] - '0');
        pos++;
    }
    return val;
}

static std::string extractField(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) {
        // Try unquoted value
        search = "\"" + key + "\":";
        pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        // Skip whitespace
        while (pos < json.size() && json[pos] == ' ') pos++;
        // Quoted value: read until the closing quote
        if (pos < json.size() && json[pos] == '"') {
            size_t end = json.find('"', pos + 1);
            if (end == std::string::npos) return "";
            return json.substr(pos + 1, end - pos - 1);
        }
        // Unquoted value: read until , or } or \n
        size_t end = json.find_first_of(",}", pos);
        if (end == std::string::npos) end = json.size();
        std::string val = json.substr(pos, end - pos);
        // Trim whitespace
        while (!val.empty() && val.front() == ' ') val.erase(0, 1);
        while (!val.empty() && val.back() == ' ') val.pop_back();
        return val;
    }
    pos += search.size();
    size_t end = json.find('\"', pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

static std::string extractMethod(const std::string& json) {
    return extractField(json, "method");
}



// Safe integer parse -- returns 0 on non-numeric input
static int safeStoi(const std::string& s) {
    if (s.empty()) return 0;
    try { return std::stoi(s); }
    catch (...) { return 0; }
}

static std::uint64_t safeStoull(const std::string& s) {
    if (s.empty()) return 0;
    try { return std::stoull(s); }
    catch (...) { return 0; }
}

static const char* debugStateName(RpcDebugRunState state) {
    switch (state) {
        case RpcDebugRunState::Detached: return "detached";
        case RpcDebugRunState::Running: return "running";
        case RpcDebugRunState::Paused: return "paused";
        case RpcDebugRunState::ResumePending: return "resume_pending";
    }
    return "detached";
}

std::string RpcServer::processRequestLine(const std::string& jsonLine) {
    if (jsonLine.size() >= 3 &&
        static_cast<unsigned char>(jsonLine[0]) == 0xEF &&
        static_cast<unsigned char>(jsonLine[1]) == 0xBB &&
        static_cast<unsigned char>(jsonLine[2]) == 0xBF) {
        return processRequestLine(jsonLine.substr(3));
    }

    std::string method = extractMethod(jsonLine);
    int id = parseId(jsonLine);

    if (method == "ping")    return handlePing(id);
    if (method == "run")     return handleRun(id, extractField(jsonLine, "script"));
    if (method == "stop")    return handleStop(id);
    if (method == "logs")    return handleLogs(id);
    if (method == "assets")  return handleAssets(id, extractField(jsonLine, "type"));
    if (method == "eval")    return handleEval(id, extractField(jsonLine, "code"));
    if (method == "getFrame") return handleGetFrame(id,
        safeStoi(extractField(jsonLine, "w")),
        safeStoi(extractField(jsonLine, "h")));
    if (method == "getState") return handleGetState(id);
    if (method == "reload")   return handleReload(id);
    if (method == "setBreakpoint") return handleDebugAction(id,
        RpcRequest{RpcSetBreakpointRequest{
            extractField(jsonLine, "source"), safeStoi(extractField(jsonLine, "line"))}});
    if (method == "removeBreakpoint") return handleDebugAction(id,
        RpcRequest{RpcRemoveBreakpointRequest{
            extractField(jsonLine, "source"), safeStoi(extractField(jsonLine, "line"))}});
    if (method == "clearBreakpoints") return handleDebugAction(id,
        RpcRequest{RpcClearBreakpointsRequest{}});
    if (method == "continue") return handleDebugAction(id,
        RpcRequest{RpcDebugResumeRequest{
            safeStoull(extractField(jsonLine, "pauseId")), RpcDebugResumeMode::Continue}});
    if (method == "stepInto") return handleDebugAction(id,
        RpcRequest{RpcDebugResumeRequest{
            safeStoull(extractField(jsonLine, "pauseId")), RpcDebugResumeMode::StepInto}});
    if (method == "stepOver") return handleDebugAction(id,
        RpcRequest{RpcDebugResumeRequest{
            safeStoull(extractField(jsonLine, "pauseId")), RpcDebugResumeMode::StepOver}});
    if (method == "stepOut") return handleDebugAction(id,
        RpcRequest{RpcDebugResumeRequest{
            safeStoull(extractField(jsonLine, "pauseId")), RpcDebugResumeMode::StepOut}});
    if (method == "inspectLocal") return handleInspectLocal(id,
        safeStoi(extractField(jsonLine, "frame")), extractField(jsonLine, "name"));
    if (method == "inspectGlobal") return handleInspectGlobal(id,
        extractField(jsonLine, "name"));
    if (method == "getDebugState") return handleGetDebugState(id);

    // Unknown method
    std::ostringstream err;
    err << "{\"id\":" << id << ",\"error\":\"Unknown method: " << jsonEscape(method) << "\"}";
    return err.str();
}

// =========================================================================
// Handlers
// =========================================================================

std::string RpcServer::handlePing(int id) {
    std::ostringstream out;
    out << "{\"id\":" << id << ",\"result\":\"ok\",\"engine\":\"CaesuraAmeKAG\"}";
    return out.str();
}

std::string RpcServer::handleRun(int id, const std::string& script) {
    if (script.empty()) {
        std::ostringstream err;
        err << "{\"id\":" << id << ",\"error\":\"Empty script\"}";
        return err.str();
    }

    pushLog("info", "Submitting scene script...");
    RpcReply reply = dispatchRequest(RpcRequest{RpcRunScriptRequest{script}});
    if (reply.status != RpcReplyStatus::Ok) {
        pushLog("error", reply.message);
        return replyError(id, reply);
    }

    pushLog("info", "Scene script submitted.");

    std::ostringstream out;
    out << "{\"id\":" << id << ",\"status\":\"ok\"}";
    return out.str();
}

std::string RpcServer::handleStop(int id) {
    RpcReply reply = dispatchRequest(RpcRequest{RpcStopRequest{}});
    if (reply.status != RpcReplyStatus::Ok) return replyError(id, reply);

    stop();
    pushLog("info", "Stop requested.");
    std::ostringstream out;
    out << "{\"id\":" << id << ",\"result\":\"ok\"}";
    return out.str();
}

std::string RpcServer::handleLogs(int id) {
    // For now, return empty. Real implementation would buffer logs.
    std::ostringstream out;
    out << "{\"id\":" << id << ",\"logs\":[]}";
    return out.str();
}

std::string RpcServer::handleAssets(int id, const std::string& type) {
    std::ostringstream out;
    out << "{\"id\":" << id << ",\"assets\":[";

    auto addDir = [&](const std::string& dir, const std::string& kind) {
        if (!type.empty() && type != kind) return;
        std::string path = "assets/" + dir;
        if (!fs::exists(path)) return;
        try {
            bool first = true;
            for (const auto& entry : fs::directory_iterator(path)) {
                if (!entry.is_regular_file()) continue;
                if (!first) out << ",";
                first = false;
                std::string name = entry.path().filename().string();
                out << "{\"path\":\"assets/" << dir << "/" << name
                    << "\",\"name\":\"" << name
                    << "\",\"type\":\"" << kind << "\"}";
            }
        } catch (...) {}
    };

    addDir("bg", "image");
    addDir("char", "image");
    addDir("ui", "image");
    addDir("bgm", "audio");
    addDir("voice", "audio");
    addDir("se", "audio");
    addDir("scripts", "script");

    out << "]}";
    return out.str();
}

std::string RpcServer::handleEval(int id, const std::string& code) {
    if (code.empty()) {
        std::ostringstream err;
        err << "{\"id\":" << id << ",\"error\":\"Empty code\"}";
        return err.str();
    }

    RpcReply reply = dispatchRequest(RpcRequest{RpcEvaluateRequest{code}});
    if (reply.status != RpcReplyStatus::Ok) return replyError(id, reply);

    const auto* result = std::get_if<RpcEvaluateResult>(&reply.payload);
    if (!result) {
        return replyError(id, RpcReply{RpcReplyStatus::Failed,
            "invalid_dispatcher_reply", "Evaluate reply did not contain a value", {}});
    }

    std::ostringstream out;
    out << "{\"id\":" << id << ",\"status\":\"ok\",\"result\":\""
        << jsonEscape(result->value) << "\"}";
    return out.str();
}

std::string RpcServer::handleGetFrame(int id, int w, int h) {
    if (w <= 0) w = 1280;
    if (h <= 0) h = 720;

    RpcReply reply = dispatchRequest(RpcRequest{RpcCaptureFrameRequest{w, h}});
    if (reply.status != RpcReplyStatus::Ok) return replyError(id, reply);

    const auto* frame = std::get_if<RpcFrameResult>(&reply.payload);
    if (!frame || frame->base64.empty()) {
        return replyError(id, RpcReply{RpcReplyStatus::Failed,
            "capture_failed", "Screenshot capture failed", {}});
    }

    std::ostringstream out;
    out << "{\"id\":" << id << ",\"frame\":\""
        << jsonEscape(frame->base64) << "\"}";
    return out.str();
}

std::string RpcServer::handleGetState(int id) {
    RpcReply reply = dispatchRequest(RpcRequest{RpcGetStateRequest{}});
    if (reply.status != RpcReplyStatus::Ok) return replyError(id, reply);

    const auto* state = std::get_if<RpcStateResult>(&reply.payload);
    if (!state) {
        return replyError(id, RpcReply{RpcReplyStatus::Failed,
            "invalid_dispatcher_reply", "State reply did not contain engine state", {}});
    }

    std::ostringstream out;
    out << "{\"id\":" << id << ",\"state\":{\"scene\":\""
        << jsonEscape(state->scene) << "\"}}";
    return out.str();
}

std::string RpcServer::handleReload(int id) {
    RpcReply reply = dispatchRequest(RpcRequest{RpcReloadScriptsRequest{}});
    if (reply.status != RpcReplyStatus::Ok) return replyError(id, reply);

    std::ostringstream out;
    out << "{\"id\":" << id << ",\"status\":\"ok\"}";
    return out.str();
}

std::string RpcServer::handleDebugAction(int id, RpcRequest request) {
    RpcReply reply = dispatchRequest(std::move(request));
    if (reply.status != RpcReplyStatus::Ok) return replyError(id, reply);

    std::ostringstream out;
    out << "{\"id\":" << id << ",\"status\":\"ok\"}";
    return out.str();
}

std::string RpcServer::handleInspectLocal(
    int id, int frame, const std::string& name) {
    RpcReply reply = dispatchRequest(
        RpcRequest{RpcInspectLocalRequest{frame, name}});
    if (reply.status != RpcReplyStatus::Ok) return replyError(id, reply);

    const auto* inspection = std::get_if<RpcInspectionResult>(&reply.payload);
    if (!inspection) {
        return replyError(id, RpcReply{RpcReplyStatus::Failed,
            "invalid_dispatcher_reply", "Local inspection reply did not contain a value", {}});
    }

    std::ostringstream out;
    out << "{\"id\":" << id << ",\"status\":\"ok\",\"value\":\""
        << jsonEscape(inspection->value) << "\"}";
    return out.str();
}

std::string RpcServer::handleInspectGlobal(int id, const std::string& name) {
    RpcReply reply = dispatchRequest(RpcRequest{RpcInspectGlobalRequest{name}});
    if (reply.status != RpcReplyStatus::Ok) return replyError(id, reply);

    const auto* inspection = std::get_if<RpcInspectionResult>(&reply.payload);
    if (!inspection) {
        return replyError(id, RpcReply{RpcReplyStatus::Failed,
            "invalid_dispatcher_reply", "Global inspection reply did not contain a value", {}});
    }

    std::ostringstream out;
    out << "{\"id\":" << id << ",\"status\":\"ok\",\"value\":\""
        << jsonEscape(inspection->value) << "\"}";
    return out.str();
}

std::string RpcServer::handleGetDebugState(int id) {
    RpcReply reply = dispatchRequest(RpcRequest{RpcGetDebugStateRequest{}});
    if (reply.status != RpcReplyStatus::Ok) return replyError(id, reply);

    const auto* state = std::get_if<RpcDebugStateResult>(&reply.payload);
    if (!state) {
        return replyError(id, RpcReply{RpcReplyStatus::Failed,
            "invalid_dispatcher_reply", "Debug state reply did not contain debug state", {}});
    }

    std::ostringstream out;
    out << "{\"id\":" << id << ",\"debug\":{\"state\":\""
        << debugStateName(state->state)
        << "\",\"source\":\"" << jsonEscape(state->source)
        << "\",\"line\":" << state->line
        << ",\"pauseId\":" << state->pauseId
        << ",\"nonYieldableHitCount\":" << state->nonYieldableHitCount
        << "}}";
    return out.str();
}

RpcReply RpcServer::dispatchRequest(RpcRequest request) const {
    std::shared_ptr<IRpcDispatcher> dispatcher;
    {
        std::lock_guard<std::mutex> lock(m_dispatcherMutex);
        dispatcher = m_dispatcher;
    }

    if (!dispatcher) {
        return {RpcReplyStatus::Unavailable, "dispatcher_unavailable",
            "RPC dispatcher is unavailable", {}};
    }

    try {
        return dispatcher->dispatch(request);
    } catch (const std::exception& ex) {
        return {RpcReplyStatus::Failed, "dispatcher_exception", ex.what(), {}};
    } catch (...) {
        return {RpcReplyStatus::Failed, "dispatcher_exception",
            "RPC dispatcher threw an unknown exception", {}};
    }
}

std::string RpcServer::replyError(int id, const RpcReply& reply) const {
    const char* status = "error";
    if (reply.status == RpcReplyStatus::InvalidRequest) status = "invalid_request";
    if (reply.status == RpcReplyStatus::Unavailable) status = "unavailable";

    const std::string message = reply.message.empty() ? "RPC request failed" : reply.message;
    const std::string code = reply.code.empty() ? "rpc_failed" : reply.code;
    std::ostringstream out;
    out << "{\"id\":" << id
        << ",\"status\":\"" << status
        << "\",\"code\":\"" << jsonEscape(code)
        << "\",\"error\":\"" << jsonEscape(message)
        << "\",\"message\":\"" << jsonEscape(message) << "\"}";
    return out.str();
}

// =========================================================================
// JSON utility
// =========================================================================

std::string RpcServer::jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

} // namespace Caesura
