// ===========================================================================
//  Caesura (AmeKAG) -- EditorServer implementation (Track 4)
// ===========================================================================

#include "EditorServer.h"
#include "ConstantTime.h"
#include "../../external/cpp-httplib/httplib.h"
#include "../debug/api/DebugLog.h"

#include <nlohmann_json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

namespace fs = std::filesystem;
namespace Caesura {

namespace {

using Json = nlohmann::json;

std::string dumpJson(const Json& value) {
    return value.dump(-1, ' ', false, Json::error_handler_t::replace);
}

std::string pathToUtf8(const fs::path& path) {
    const auto value = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool isAnimationAsset(const fs::path& path) {
    const std::string extension = lowerAscii(path.extension().string());
    if (extension == ".png" || extension == ".jpg" ||
        extension == ".jpeg" || extension == ".bmp" ||
        extension == ".moc" || extension == ".moc3" ||
        extension == ".json") {
        return true;
    }

    return lowerAscii(path.filename().string()).find(".model") !=
        std::string::npos;
}

bool readOptionalFloat(const Json& body,
                       const char* field,
                       float defaultValue,
                       float& result,
                       std::string& error) {
    const auto it = body.find(field);
    if (it == body.end()) {
        result = defaultValue;
        return true;
    }
    if (!it->is_number()) {
        error = std::string(field) + " must be a finite number";
        return false;
    }

    double value = 0.0;
    try {
        value = it->get<double>();
    } catch (const Json::exception&) {
        error = std::string(field) + " must be a finite number";
        return false;
    }

    constexpr double maxFloat =
        static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(value) || value < -maxFloat || value > maxFloat) {
        error = std::string(field) + " must be a finite number";
        return false;
    }

    result = static_cast<float>(value);
    return true;
}

RpcReply invalidAnimationRequest(const std::string& message) {
    return {RpcReplyStatus::InvalidRequest, "invalid_animation_request",
        message, {}};
}

void setDispatchError(httplib::Response& response, const RpcReply& reply) {
    const char* status = "error";
    int httpStatus = 500;
    if (reply.status == RpcReplyStatus::InvalidRequest) {
        status = "invalid_request";
        httpStatus = 400;
    } else if (reply.status == RpcReplyStatus::Unavailable) {
        status = "unavailable";
        httpStatus = 503;
    } else if (reply.status == RpcReplyStatus::Busy) {
        status = "busy";
        httpStatus = 503;
    }

    const std::string code = reply.code.empty() ? "rpc_failed" : reply.code;
    const std::string message = reply.message.empty() ? "RPC request failed" : reply.message;
    response.status = httpStatus;
    response.set_content(dumpJson({
        {"status", status},
        {"code", code},
        {"error", message},
        {"message", message},
    }), "application/json");
}

RpcReply invalidDispatcherReply(const std::string& message) {
    return {RpcReplyStatus::Failed, "invalid_dispatcher_reply", message, {}};
}

} // namespace

// =========================================================================
// Lifecycle
// =========================================================================

EditorServer::EditorServer() = default;

EditorServer::~EditorServer() {
    stop();
}

bool EditorServer::start(int port) {
    if (m_running) {
        printf("[EditorServer] Already running on port %d\n", m_port);
        return true;
    }

    if (m_thread.joinable()) stop();

    m_server = std::make_unique<httplib::Server>();
    const int boundPort = port == 0
        ? m_server->bind_to_any_port("127.0.0.1")
        : (m_server->bind_to_port("127.0.0.1", port) ? port : -1);
    if (boundPort < 0) {
        m_server.reset();
        m_port = 0;
        return false;
    }

    m_port = boundPort;
    m_running = true;
    m_thread = std::thread(&EditorServer::serverLoop, this, boundPort);

    // Give the server a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    if (m_running) {
        printf("[EditorServer] Started on http://localhost:%d\n", m_port);
        return true;
    }
    return false;
}

void EditorServer::setDispatcher(std::shared_ptr<IRpcDispatcher> dispatcher) {
    std::lock_guard<std::mutex> lock(m_dispatcherMutex);
    m_dispatcher = std::move(dispatcher);
}

void EditorServer::setAuthToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(m_dispatcherMutex);
    m_authToken = token;
}

RpcReply EditorServer::dispatchRequest(RpcRequest request) const {
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

void EditorServer::stop() {
    const bool wasRunning = m_running.exchange(false);
    if (m_server) m_server->stop();
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_server.reset();
    if (wasRunning) printf("[EditorServer] Stopped.\n");
}

// =========================================================================
// Log buffer
// =========================================================================

void EditorServer::pushLog(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_logMutex);

    // Timestamp
    time_t now = time(nullptr);
    char timeBuf[32];
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", localtime(&now));

    m_logs.push_back({level, message, timeBuf});
    if (m_logs.size() > MAX_LOGS) {
        m_logs.erase(m_logs.begin());
    }
}

// =========================================================================
// Server thread
// =========================================================================

void EditorServer::serverLoop(int port) {
    httplib::Server& svr = *m_server;

    // ---------------------------------------------------------------------
    // CORS middleware - allow web editor from any origin
    // ---------------------------------------------------------------------
    svr.set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        // Only allow same-origin / localhost web editor frontends. A bare "*"
        // would let any website drive the local editor over CORS.
        const std::string origin = req.get_header_value("Origin");
        // Exact host match: http://localhost[:port] or http://127.0.0.1[:port].
        // A raw prefix match would let http://localhost.evil.com (attacker-owned
        // parent domain) pass and read/drive the local editor from a browser.
        bool localOrigin = origin.empty();
        if (!localOrigin) {
            // Only http origins are eligible (https is rejected by design);
            // guard length so substr cannot throw on short headers such as
            // "Origin: http:/".
            if (origin.size() < 7 || origin.rfind("http://", 0) != 0) {
                localOrigin = false;
            } else {
                const std::string suffix = origin.substr(7);  // strip "http://"
                const auto slash = suffix.find('/');
                const std::string authority = slash == std::string::npos
                    ? suffix : suffix.substr(0, slash);
                const auto colon = authority.find(':');
                const std::string host = colon == std::string::npos
                    ? authority : authority.substr(0, colon);
                localOrigin = (host == "localhost" || host == "127.0.0.1");
            }
        }
        if (!localOrigin) {
            res.status = 403;
            res.set_content("{\"error\":\"Origin not allowed\"}", "application/json");
            return httplib::Server::HandlerResponse::Handled;
        }
        if (!origin.empty()) {
            res.set_header("Access-Control-Allow-Origin", origin.c_str());
            res.set_header("Vary", "Origin");
        }
        // [Review R-3] API-token gate: the editor binds only to loopback
        // (127.0.0.1), so no external host can reach it — the residual risk is
        // another LOCAL process driving /api/run, /api/eval, etc. to execute
        // arbitrary Lua. Mitigation is the bearer-token gate below: when
        // m_authToken (CAESURA_EDITOR_TOKEN) is set, every /api/* request must
        // present "Authorization: Bearer <token>" or get 401. When the token is
        // empty the gate is open, preserving the existing editor workflow and
        // tests ("default open + mandatory auth once configured" — backwards
        // compatible hardening, not default-deny). CORS preflight (OPTIONS)
        // carries no Authorization header and must not be gated, or the browser
        // web editor breaks; the actual request behind the preflight is checked.
        if (req.method != "OPTIONS") {
            const std::string auth = req.get_header_value("Authorization");
            std::string token;
            {
                std::lock_guard<std::mutex> lock(m_dispatcherMutex);
                token = m_authToken;
            }
            if (!token.empty()) {
                const std::string expected = "Bearer " + token;
                if (!constantTimeEquals(auth, expected)) {
                    res.status = 401;
                    res.set_content("{\"error\":\"Unauthorized\"}", "application/json");
                    return httplib::Server::HandlerResponse::Handled;
                }
            }
        }
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // ---------------------------------------------------------------------
    // GET /api/ping -- health check
    // ---------------------------------------------------------------------
    svr.Get("/api/ping", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\",\"engine\":\"CaesuraAmeKAG\"}", "application/json");
    });

    // ---------------------------------------------------------------------
    // GET /api/status -- detailed engine status (R1.1)
    // ---------------------------------------------------------------------
    svr.Get("/api/status", [this](const httplib::Request&, httplib::Response& res) {
        RpcReply reply = dispatchRequest(RpcRequest{RpcStatusRequest{}});
        if (reply.status != RpcReplyStatus::Ok) {
            setDispatchError(res, reply);
            return;
        }

        const auto* status = std::get_if<RpcStatusResult>(&reply.payload);
        if (!status) {
            setDispatchError(res, invalidDispatcherReply(
                "Status reply did not contain runtime status"));
            return;
        }

        const char* luaOk = status->luaReady ? "true" : "false";
        char buf[256];
        snprintf(buf, sizeof(buf),
            "{\"status\":\"ok\",\"engine\":\"CaesuraAmeKAG\",\"lua\":%s,\"port\":%d}",
            luaOk, m_port);
        res.set_content(buf, "application/json");
    });

    // ---------------------------------------------------------------------
    // GET /api/assets -- list project assets
    // ---------------------------------------------------------------------
    svr.Get("/api/assets", [](const httplib::Request& req, httplib::Response& res) {
        std::string type = req.get_param_value("type");

        // Build JSON array
        Json arr = Json::array();

        // Each scanned disk directory maps to a coarse type (image/audio/script)
        // shared with the editor's coarse filter, and a per-directory kind
        // (bg|fg|bgm|se|voice|...) that Scene Builder uses to pick assets for
        // a specific slot. The ?type= filter accepts EITHER the coarse type
        // ("image", "audio", "script" — backward compatible) OR the per-dir
        // kind ("bg", "fg", "bgm", "se", ...); omitted means every directory.
        const std::vector<std::pair<std::string, std::string>> dirs = {
            {"bg", "image"},     // background images
            {"fg", "image"},     // foreground / character sprites
            {"char", "image"},   // legacy character portraits
            {"ui", "image"},     // UI elements
            {"bgm", "audio"},    // background music
            {"voice", "audio"},  // voice lines
            {"se", "audio"},     // sound effects
            {"scripts", "script"},
        };

        auto includeDir = [&](const std::string& dir, const std::string& coarse) {
            if (type.empty()) return true;
            if (type == coarse) return true;  // coarse category filter
            if (type == dir) return true;     // per-directory slot filter
            return false;
        };

        for (const auto& [dir, coarse] : dirs) {
            if (!includeDir(dir, coarse)) continue;
            std::string path = "assets/" + dir;
            if (!fs::exists(path)) continue;
            try {
                for (const auto& entry : fs::directory_iterator(path)) {
                    if (!entry.is_regular_file()) continue;
                    std::string name = entry.path().filename().string();
                    std::string relPath = "assets/" + dir + "/" + name;
                    Json obj;
                    obj["path"] = relPath;
                    obj["name"] = name;
                    obj["type"] = coarse;
                    obj["kind"] = dir;
                    arr.push_back(obj);
                }
            } catch (...) {}
        }

        res.set_content(dumpJson(arr), "application/json");
    });

    // ---------------------------------------------------------------------
    // POST /api/run -- execute Lua script
    // ---------------------------------------------------------------------
    svr.Post("/api/run", [this](const httplib::Request& req, httplib::Response& res) {
        std::string script = req.body;
        if (script.empty()) {
            res.set_content("{\"error\":\"Empty script\"}", "application/json");
            res.status = 400;
            return;
        }

        pushLog("info", "Running scene script...");
        RpcReply reply = dispatchRequest(RpcRequest{RpcRunScriptRequest{script}});
        if (reply.status != RpcReplyStatus::Ok) {
            pushLog("error", reply.message);
            setDispatchError(res, reply);
            return;
        }

        pushLog("info", "Scene script completed.");
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    // ---------------------------------------------------------------------
    // POST /api/stop -- stop execution
    // ---------------------------------------------------------------------
    svr.Post("/api/stop", [this](const httplib::Request&, httplib::Response& res) {
        RpcReply reply = dispatchRequest(RpcRequest{RpcStopRequest{}});
        if (reply.status != RpcReplyStatus::Ok) {
            setDispatchError(res, reply);
            return;
        }

        pushLog("info", "Stop requested.");
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    // ---------------------------------------------------------------------
    // POST /api/reload -- request an owner-thread script reload
    // ---------------------------------------------------------------------
    svr.Post("/api/reload", [this](const httplib::Request&, httplib::Response& res) {
        RpcReply reply = dispatchRequest(RpcRequest{RpcReloadScriptsRequest{}});
        if (reply.status != RpcReplyStatus::Ok) {
            setDispatchError(res, reply);
            return;
        }

        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    // ---------------------------------------------------------------------
    // GET /api/logs -- recent log entries
    // ---------------------------------------------------------------------
    svr.Get("/api/logs", [this](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(m_logMutex);

        Json arr = Json::array();
        for (const auto& entry : m_logs) {
            Json obj;
            obj["level"] = entry.level;
            obj["message"] = std::string(entry.message);
            obj["time"] = entry.time;
            arr.push_back(obj);
        }
        res.set_content(dumpJson(arr), "application/json");
    });

    // ---------------------------------------------------------------------

    // ---------------------------------------------------------------------
    // GET /api/live2d/models -- list available Live2D models (R1.2)
    // ---------------------------------------------------------------------
    svr.Get("/api/live2d/models", [](const httplib::Request&, httplib::Response& res) {
        struct ModelEntry {
            std::string name;
            std::string path;
        };

        std::vector<ModelEntry> models;
        const char* dirs[] = {"models", "assets/models", "assets/live2d"};
        for (const char* dir : dirs) {
            if (!fs::exists(dir)) continue;
            try {
                for (const auto& entry : fs::directory_iterator(dir)) {
                    if (!entry.is_regular_file()) continue;
                    if (!isAnimationAsset(entry.path())) continue;
                    models.push_back({
                        pathToUtf8(entry.path().filename()),
                        pathToUtf8(entry.path()),
                    });
                }
            } catch (...) {}
        }

        std::sort(models.begin(), models.end(),
            [](const ModelEntry& lhs, const ModelEntry& rhs) {
                return lhs.path < rhs.path;
            });

        Json response = Json::array();
        for (const auto& model : models) {
            response.push_back({
                {"name", model.name},
                {"path", model.path},
            });
        }
        res.set_content(dumpJson(response), "application/json");
    });

    // ---------------------------------------------------------------------
    // POST /api/live2d/load -- load a Live2D model (R1.2)
    // ---------------------------------------------------------------------
    svr.Post("/api/live2d/load", [this](const httplib::Request& req, httplib::Response& res) {
        Json body;
        try {
            body = Json::parse(req.body);
        } catch (const Json::exception&) {
            setDispatchError(res, invalidAnimationRequest(
                "Request body must be a valid JSON object"));
            return;
        }

        if (!body.is_object()) {
            setDispatchError(res, invalidAnimationRequest(
                "Request body must be a JSON object"));
            return;
        }

        const auto modelPathIt = body.find("modelPath");
        if (modelPathIt == body.end() || !modelPathIt->is_string() ||
            modelPathIt->get_ref<const std::string&>().empty()) {
            setDispatchError(res, invalidAnimationRequest(
                "modelPath must be a non-empty string"));
            return;
        }

        RpcLoadAnimationRequest request;
        request.modelPath = modelPathIt->get<std::string>();
        std::string error;
        if (!readOptionalFloat(body, "x", 0.0f, request.x, error) ||
            !readOptionalFloat(body, "y", 0.0f, request.y, error) ||
            !readOptionalFloat(body, "scale", 1.0f, request.scale, error)) {
            setDispatchError(res, invalidAnimationRequest(error));
            return;
        }
        if (request.scale <= 0.0f) {
            setDispatchError(res, invalidAnimationRequest(
                "scale must be greater than zero"));
            return;
        }

        const auto showIt = body.find("show");
        if (showIt != body.end()) {
            if (!showIt->is_boolean()) {
                setDispatchError(res, invalidAnimationRequest(
                    "show must be a boolean"));
                return;
            }
            request.show = showIt->get<bool>();
        }

        RpcReply reply = dispatchRequest(RpcRequest{std::move(request)});
        if (reply.status != RpcReplyStatus::Ok) {
            setDispatchError(res, reply);
            return;
        }

        const auto* animation = std::get_if<RpcAnimationResult>(&reply.payload);
        if (animation && animation->modelId > 0) {
            res.set_content(dumpJson({
                {"status", "ok"},
                {"modelId", animation->modelId},
                {"name", animation->name},
            }), "application/json");
        } else {
            setDispatchError(res, invalidDispatcherReply(
                "Animation reply did not contain a valid model"));
        }
    });


    // ---------------------------------------------------------------------
    // POST /api/eval -- evaluate Lua code and return its stringified value
    // ---------------------------------------------------------------------
    svr.Post("/api/eval", [this](const httplib::Request& req, httplib::Response& res) {
        std::string code = req.body;
        if (code.empty()) {
            res.set_content("{\"error\":\"Empty code\"}", "application/json");
            res.status = 400;
            return;
        }
        RpcReply reply = dispatchRequest(RpcRequest{RpcEvaluateRequest{code}});
        if (reply.status != RpcReplyStatus::Ok) {
            setDispatchError(res, reply);
            return;
        }
        const auto* result = std::get_if<RpcEvaluateResult>(&reply.payload);
        if (!result) {
            setDispatchError(res, invalidDispatcherReply(
                "Eval reply did not contain a result value"));
            return;
        }
        res.set_content(dumpJson({
            {"status", "ok"},
            {"result", result->value},
        }), "application/json");
    });

    // ---------------------------------------------------------------------
    // GET /api/debug/getState -- current scene / debug state
    // ---------------------------------------------------------------------
    svr.Get("/api/debug/getState", [this](const httplib::Request&, httplib::Response& res) {
        RpcReply reply = dispatchRequest(RpcRequest{RpcGetStateRequest{}});
        if (reply.status != RpcReplyStatus::Ok) {
            setDispatchError(res, reply);
            return;
        }
        const auto* state = std::get_if<RpcStateResult>(&reply.payload);
        if (!state) {
            setDispatchError(res, invalidDispatcherReply(
                "State reply did not contain a scene"));
            return;
        }
        res.set_content(dumpJson({
            {"status", "ok"},
            {"scene", state->scene},
            {"token_index", state->tokenIndex},
            {"nvl_mode", state->nvlMode},
            {"language", state->language},
            {"backlog_count", state->backlogCount},
            {"layer_count", state->layerCount},
            {"current_cmd", state->currentCmd},
        }), "application/json");
    });

    // ---------------------------------------------------------------------
    // GET /api/state -- canonical engine state for the IDE preview panel
    // (same payload as /api/debug/getState; round 18).
    // ---------------------------------------------------------------------
    svr.Get("/api/state", [this](const httplib::Request&, httplib::Response& res) {
        RpcReply reply = dispatchRequest(RpcRequest{RpcGetStateRequest{}});
        if (reply.status != RpcReplyStatus::Ok) {
            setDispatchError(res, reply);
            return;
        }
        const auto* state = std::get_if<RpcStateResult>(&reply.payload);
        if (!state) {
            setDispatchError(res, invalidDispatcherReply(
                "State reply did not contain engine state"));
            return;
        }
        res.set_content(dumpJson({
            {"status", "ok"},
            {"scene", state->scene},
            {"token_index", state->tokenIndex},
            {"nvl_mode", state->nvlMode},
            {"language", state->language},
            {"backlog_count", state->backlogCount},
            {"layer_count", state->layerCount},
            {"current_cmd", state->currentCmd},
        }), "application/json");
    });

    // ---------------------------------------------------------------------
    // GET /api/sma/validate?path=... -- SMA asset validation through the
    // engine's shared checker (kag.sma_check). Returns ok + violations +
    // a structure summary for the IDE SMA asset panel (round 19).
    // ---------------------------------------------------------------------
    svr.Get("/api/sma/validate", [this](const httplib::Request& req, httplib::Response& res) {
        const std::string path = req.get_param_value("path");
        if (path.empty()) {
            res.set_content("{\"error\":\"Missing path parameter\"}", "application/json");
            res.status = 400;
            return;
        }
        RpcReply reply = dispatchRequest(RpcRequest{RpcSmaValidateRequest{path}});
        if (reply.status != RpcReplyStatus::Ok) {
            setDispatchError(res, reply);
            return;
        }
        const auto* v = std::get_if<RpcSmaValidateResult>(&reply.payload);
        if (!v) {
            setDispatchError(res, invalidDispatcherReply(
                "SMA validate reply missing result"));
            return;
        }
        res.set_content(dumpJson({
            {"status", "ok"},
            {"ok", v->ok},
            {"errors", v->errors},
            {"meta", v->meta},
        }), "application/json");
    });

    // ---------------------------------------------------------------------
    // GET /api/pick?x=&y= -- IDE preview-frame hit test (round 23): returns
    // the Lua layer-tree nodes containing the window pixel, bottom-to-top.
    // ---------------------------------------------------------------------
    svr.Get("/api/pick", [this](const httplib::Request& req, httplib::Response& res) {
        int x = 0, y = 0;
        if (req.has_param("x")) x = std::atoi(req.get_param_value("x").c_str());
        if (req.has_param("y")) y = std::atoi(req.get_param_value("y").c_str());
        if (x < 0 || y < 0 || x > 8192 || y > 8192) {
            res.set_content("{\"error\":\"Invalid pick coordinates\"}", "application/json");
            res.status = 400;
            return;
        }
        RpcReply reply = dispatchRequest(RpcRequest{RpcPickRequest{x, y}});
        if (reply.status != RpcReplyStatus::Ok) {
            setDispatchError(res, reply);
            return;
        }
        const auto* p = std::get_if<RpcPickResult>(&reply.payload);
        if (!p) {
            setDispatchError(res, invalidDispatcherReply(
                "Pick reply missing result"));
            return;
        }
        res.set_content(dumpJson({
            {"status", "ok"},
            {"hits", p->hits},
        }), "application/json");
    });

    // ---------------------------------------------------------------------
    // GET /api/stats -- engine runtime stats for the IDE panel (round 28).
    // ---------------------------------------------------------------------
    svr.Get("/api/stats", [this](const httplib::Request&, httplib::Response& res) {
        RpcReply reply = dispatchRequest(RpcRequest{RpcStatsRequest{}});
        if (reply.status != RpcReplyStatus::Ok) {
            setDispatchError(res, reply);
            return;
        }
        const auto* s = std::get_if<RpcStatsResult>(&reply.payload);
        if (!s) {
            setDispatchError(res, invalidDispatcherReply(
                "Stats reply missing result"));
            return;
        }
        res.set_content(dumpJson({
            {"status", "ok"},
            {"texture_budget_mb", s->textureBudgetMB},
            {"texture_tier", s->textureTier},
            {"texture_tier_name", s->textureTierName},
            {"mesh_count", s->meshCount},
            {"job_workers", s->jobWorkers},
            {"job_pending", s->jobPending},
            {"lua_kb", s->luaKb},
        }), "application/json");
    });

    // ---------------------------------------------------------------------
    // POST /api/sma/save -- validate + write back an SMA asset (round 26).
    // Body: {"path": "...", "content": "..."}; path restricted to assets/.
    // ---------------------------------------------------------------------
    svr.Post("/api/sma/save", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            const std::string path = body.value("path", std::string());
            const std::string content = body.value("content", std::string());
            if (path.empty() || content.empty()) {
                res.set_content("{\"error\":\"Missing path or content\"}", "application/json");
                res.status = 400;
                return;
            }
            RpcReply reply = dispatchRequest(
                RpcRequest{RpcSmaSaveRequest{path, content}});
            if (reply.status != RpcReplyStatus::Ok) {
                setDispatchError(res, reply);
                return;
            }
            const auto* v = std::get_if<RpcSmaSaveResult>(&reply.payload);
            if (!v) {
                setDispatchError(res, invalidDispatcherReply(
                    "SMA save reply missing result"));
                return;
            }
            res.set_content(dumpJson({
                {"status", "ok"},
                {"ok", v->ok},
                {"errors", v->errors},
            }), "application/json");
        } catch (const std::exception&) {
            res.set_content("{\"error\":\"Invalid JSON body\"}", "application/json");
            res.status = 400;
        }
    });

    // ---------------------------------------------------------------------
    // GET /api/debug/getFrame -- capture the current frame as base64 PNG
    // ---------------------------------------------------------------------
    svr.Get("/api/debug/getFrame", [this](const httplib::Request& req, httplib::Response& res) {
        int w = 1280, h = 720;
        if (req.has_param("w")) w = std::atoi(req.get_param_value("w").c_str());
        if (req.has_param("h")) h = std::atoi(req.get_param_value("h").c_str());
        if (w <= 0 || h <= 0 || w > 8192 || h > 8192) {
            res.set_content("{\"error\":\"Invalid frame dimensions\"}", "application/json");
            res.status = 400;
            return;
        }
        RpcReply reply = dispatchRequest(
            RpcRequest{RpcCaptureFrameRequest{w, h}});
        if (reply.status != RpcReplyStatus::Ok) {
            setDispatchError(res, reply);
            return;
        }
        const auto* frame = std::get_if<RpcFrameResult>(&reply.payload);
        if (!frame) {
            setDispatchError(res, invalidDispatcherReply(
                "Frame reply did not contain a capture"));
            return;
        }
        res.set_content(dumpJson({
            {"status", "ok"},
            {"base64", frame->base64},
        }), "application/json");
    });

    // ---------------------------------------------------------------------
    // POST /api/debug/setBreakpoint -- set a source line breakpoint
    // ---------------------------------------------------------------------
    svr.Post("/api/debug/setBreakpoint", [this](const httplib::Request& req, httplib::Response& res) {
        Json body;
        try {
            body = Json::parse(req.body);
        } catch (const Json::exception&) {
            setDispatchError(res, invalidAnimationRequest(
                "Request body must be a valid JSON object"));
            return;
        }
        if (!body.is_object() || !body.contains("source") ||
            !body["source"].is_string() || !body.contains("line") ||
            !body["line"].is_number_integer()) {
            setDispatchError(res, invalidAnimationRequest(
                "source (string) and line (integer) are required"));
            return;
        }
        std::int64_t line64 = 0;
        try {
            line64 = body["line"].get<std::int64_t>();
        } catch (const Json::exception&) {
            setDispatchError(res, invalidAnimationRequest(
                "line must be a positive integer in int32 range"));
            return;
        }
        if (line64 <= 0 || line64 > (std::numeric_limits<int>::max)()) {
            setDispatchError(res, invalidAnimationRequest(
                "line must be a positive integer in int32 range"));
            return;
        }
        const int line = static_cast<int>(line64);
        RpcReply reply = dispatchRequest(RpcRequest{RpcSetBreakpointRequest{
            body["source"].get<std::string>(), line}});
        if (reply.status != RpcReplyStatus::Ok) {
            setDispatchError(res, reply);
            return;
        }
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    // ---------------------------------------------------------------------
    // POST /api/debug/removeBreakpoint -- remove a source line breakpoint
    // ---------------------------------------------------------------------
    svr.Post("/api/debug/removeBreakpoint", [this](const httplib::Request& req, httplib::Response& res) {
        Json body;
        try {
            body = Json::parse(req.body);
        } catch (const Json::exception&) {
            setDispatchError(res, invalidAnimationRequest(
                "Request body must be a valid JSON object"));
            return;
        }
        if (!body.is_object() || !body.contains("source") ||
            !body["source"].is_string() || !body.contains("line") ||
            !body["line"].is_number_integer()) {
            setDispatchError(res, invalidAnimationRequest(
                "source (string) and line (integer) are required"));
            return;
        }
        std::int64_t line64 = 0;
        try {
            line64 = body["line"].get<std::int64_t>();
        } catch (const Json::exception&) {
            setDispatchError(res, invalidAnimationRequest(
                "line must be a positive integer in int32 range"));
            return;
        }
        if (line64 <= 0 || line64 > (std::numeric_limits<int>::max)()) {
            setDispatchError(res, invalidAnimationRequest(
                "line must be a positive integer in int32 range"));
            return;
        }
        const int line = static_cast<int>(line64);
        RpcReply reply = dispatchRequest(RpcRequest{RpcRemoveBreakpointRequest{
            body["source"].get<std::string>(), line}});
        if (reply.status != RpcReplyStatus::Ok) {
            setDispatchError(res, reply);
            return;
        }
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    // ---------------------------------------------------------------------
    // POST /api/debug/clearBreakpoints -- remove all breakpoints
    // ---------------------------------------------------------------------
    svr.Post("/api/debug/clearBreakpoints", [this](const httplib::Request&, httplib::Response& res) {
        RpcReply reply = dispatchRequest(RpcRequest{RpcClearBreakpointsRequest{}});
        if (reply.status != RpcReplyStatus::Ok) {
            setDispatchError(res, reply);
            return;
        }
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    // ---------------------------------------------------------------------
    // POST /api/debug/continue -- resume a paused debugger run
    // ---------------------------------------------------------------------
    svr.Post("/api/debug/continue", [this](const httplib::Request& req, httplib::Response& res) {
        RpcDebugResumeRequest request;
        request.mode = RpcDebugResumeMode::Continue;
        Json body;
        try {
            body = Json::parse(req.body);
        } catch (const Json::exception&) {
            // Empty/absent body is allowed; defaults apply.
        }
        if (body.is_object() && body.contains("pauseId") &&
            body["pauseId"].is_number_unsigned()) {
            try {
                request.pauseId = body["pauseId"].get<std::uint64_t>();
            } catch (const Json::exception&) {
                // Malformed pauseId: fall through with default 0.
            }
        }
        RpcReply reply = dispatchRequest(RpcRequest{std::move(request)});
        if (reply.status != RpcReplyStatus::Ok) {
            setDispatchError(res, reply);
            return;
        }
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    // ---------------------------------------------------------------------
    // POST /api/debug/stepInto|stepOver|stepOut -- single-step a paused
    // debugger run (Sprint 4b). Same contract as /continue but with the
    // matching RpcDebugResumeMode.
    // ---------------------------------------------------------------------
    const auto stepHandler = [this](RpcDebugResumeMode mode) {
        return [this, mode](const httplib::Request& req, httplib::Response& res) {
            RpcDebugResumeRequest request;
            request.mode = mode;
            Json body;
            try {
                body = Json::parse(req.body);
            } catch (const Json::exception&) {
                // Empty/absent body is allowed; defaults apply.
            }
            if (body.is_object() && body.contains("pauseId") &&
                body["pauseId"].is_number_unsigned()) {
                try {
                    request.pauseId = body["pauseId"].get<std::uint64_t>();
                } catch (const Json::exception&) {
                    // Malformed pauseId: fall through with default 0.
                }
            }
            RpcReply reply = dispatchRequest(RpcRequest{std::move(request)});
            if (reply.status != RpcReplyStatus::Ok) {
                setDispatchError(res, reply);
                return;
            }
            res.set_content("{\"status\":\"ok\"}", "application/json");
        };
    };
    svr.Post("/api/debug/stepInto", stepHandler(RpcDebugResumeMode::StepInto));
    svr.Post("/api/debug/stepOver", stepHandler(RpcDebugResumeMode::StepOver));
    svr.Post("/api/debug/stepOut", stepHandler(RpcDebugResumeMode::StepOut));

    // ---------------------------------------------------------------------
    // GET /api/debug/inspect -- inspect a Lua local or global variable
    // ---------------------------------------------------------------------
    svr.Get("/api/debug/inspect", [this](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("name") || req.get_param_value("name").empty()) {
            res.set_content("{\"error\":\"name parameter is required\"}", "application/json");
            res.status = 400;
            return;
        }
        const std::string name = req.get_param_value("name");
        int frame = 0;
        if (req.has_param("frame")) frame = std::atoi(req.get_param_value("frame").c_str());
        RpcReply reply;
        if (req.has_param("global")) {
            reply = dispatchRequest(RpcRequest{RpcInspectGlobalRequest{name}});
        } else {
            reply = dispatchRequest(RpcRequest{RpcInspectLocalRequest{frame, name}});
        }
        if (reply.status != RpcReplyStatus::Ok) {
            setDispatchError(res, reply);
            return;
        }
        const auto* inspection = std::get_if<RpcInspectionResult>(&reply.payload);
        if (!inspection) {
            setDispatchError(res, invalidDispatcherReply(
                "Inspect reply did not contain a value"));
            return;
        }
        res.set_content(dumpJson({
            {"status", "ok"},
            {"value", inspection->value},
        }), "application/json");
    });
    // ---------------------------------------------------------------------
    // POST /api/build -- one-click CARC packaging (R1.3)
    // ---------------------------------------------------------------------
    svr.Post("/api/build", [this](const httplib::Request& req, httplib::Response& res) {
        std::string outputPath = "build/game.carc";
        std::string keyPath = "build/game.key";

        // Parse optional outputPath and keyPath from body. An empty body keeps
        // the defaults (backward compatible); malformed JSON is a 400. The old
        // hand-rolled scanner truncated values containing escaped quotes and
        // mis-matched on literal "outputPath" inside a value.
        Json body;
        try {
            body = req.body.empty() ? Json::object() : Json::parse(req.body);
        } catch (const Json::exception&) {
            res.set_content("{\"error\":\"Request body must be valid JSON\"}",
                            "application/json");
            res.status = 400;
            return;
        }
        if (!body.is_object()) {
            res.set_content("{\"error\":\"Request body must be a JSON object\"}",
                            "application/json");
            res.status = 400;
            return;
        }
        if (body.contains("outputPath")) {
            if (!body["outputPath"].is_string()) {
                res.set_content("{\"error\":\"outputPath must be a string\"}", "application/json");
                res.status = 400;
                return;
            }
            const std::string v = body["outputPath"].get<std::string>();
            if (!v.empty()) outputPath = v;
        }
        if (body.contains("keyPath")) {
            if (!body["keyPath"].is_string()) {
                res.set_content("{\"error\":\"keyPath must be a string\"}", "application/json");
                res.status = 400;
                return;
            }
            const std::string v = body["keyPath"].get<std::string>();
            if (!v.empty()) keyPath = v;
        }

        // Security: confine output paths under build/ -- reject absolute paths
        // and ".." escapes so a local caller cannot overwrite arbitrary files.
        auto confineToBuild = [](std::string p) -> std::string {
            if (p.empty()) return {};
            for (auto& ch : p) {
                if (ch == static_cast<char>(92)) ch = '/';  // normalize backslash
            }
            if (p.find("..") != std::string::npos) return {};
            if (p.find(':') != std::string::npos) return {};  // drive / scheme
            if (p[0] == '/') return {};                        // absolute
            // Force everything under build/ regardless of what the caller sent.
            if (p.rfind("build/", 0) != 0) {
                p = "build/" + p;
            }
            return p;
        };
        outputPath = confineToBuild(outputPath);
        keyPath = confineToBuild(keyPath);
        if (outputPath.empty() || keyPath.empty()) {
            res.set_content(
                "{\"error\":\"outputPath/keyPath must be relative paths under build/\"}",
                "application/json");
            res.status = 400;
            return;
        }

        // Collect files from scripts/ and assets/
        std::vector<std::pair<std::string, std::string>> files; // relPath, diskPath
        for (const char* dir : {"scripts", "assets"}) {
            if (!fs::exists(dir)) continue;
            try {
                for (const auto& entry : fs::recursive_directory_iterator(dir)) {
                    if (!entry.is_regular_file()) continue;
                    std::string rel = entry.path().string();
                    // Normalize to forward slashes
                    for (auto& c : rel) if (c == '\\') c = '/';
                    files.push_back({rel, entry.path().string()});
                }
            } catch (...) {}
        }

        if (files.empty()) {
            res.set_content("{\"error\":\"No files to package\"}", "application/json");
            res.status = 400;
            return;
        }

        // Create output directory
        fs::create_directories("build");

        auto writer = m_archiveWriterFactory ? m_archiveWriterFactory() : nullptr;
        if (!writer) {
            res.set_content("{\"error\":\"Archive writer is not configured\"}", "application/json");
            res.status = 503;
            return;
        }
        if (!writer->create(outputPath, keyPath, keyPath + ".pub")) {
            res.set_content("{\"error\":\"Failed to create CARC archive\"}", "application/json");
            res.status = 500;
            return;
        }

        for (const auto& [relPath, diskPath] : files) {
            std::ifstream ifs(diskPath, std::ios::binary);
            if (!ifs.is_open()) continue;
            std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)),
                                       std::istreambuf_iterator<char>());
            writer->addFile(relPath, data.data(), data.size());
        }

        if (!writer->finalize()) {
            res.set_content("{\"error\":\"Failed to finalize CARC archive\"}", "application/json");
            res.status = 500;
            return;
        }

        auto fileSize = fs::file_size(outputPath);
        res.set_content(dumpJson({
            {"status", "ok"},
            {"path", outputPath},
            {"size", static_cast<unsigned long long>(fileSize)},
            {"files", files.size()},
        }), "application/json");
    });

    // ---------------------------------------------------------------------
    // Static file serving -- web editor frontend
    // Static file serving -- web editor frontend
    // ---------------------------------------------------------------------
    // Serve the single-file editor (web-editor/dist/index.html). We read the
    // file through our own ifstream instead of httplib set_mount_point: on
    // Windows, non-ASCII (e.g. CJK) directory paths in the mount dir are
    // re-encoded by the narrow-string CRT layer and silently 404. The
    // explicit handler keeps the path as-is end to end.
    if (!m_webRoot.empty() && fs::exists(m_webRoot)) {
        const auto indexFile = (fs::path(m_webRoot) / "index.html").string();
        auto serveIndex = [indexFile](const httplib::Request&, httplib::Response& res) {
            std::ifstream f(indexFile, std::ios::binary);
            if (!f) {
                res.status = 404;
                res.set_content("index.html not found", "text/plain");
                return;
            }
            std::string body((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
            res.set_content(body, "text/html; charset=utf-8");
        };
        svr.Get("/", serveIndex);
        svr.Get("/index.html", serveIndex);
        printf("[EditorServer] Serving web editor from: %s\n", m_webRoot.c_str());
    }

    // ---------------------------------------------------------------------
    // Project Manager endpoints (Sprint 2, task book §6.3)
    //  GET    /api/project/templates   -> [{id,name,description,dir}]
    //  GET    /api/project/list        -> [{path,name,template,modified}]
    //  POST   /api/project/create      -> {template,name,path}
    //  POST   /api/project/duplicate   -> {srcPath,name}
    //  POST   /api/project/import      -> {srcPath,name} (srcPath is ANY
    //           on-disk directory; name must be sanitized)
    //
    // These are EDITOR-ORIENTED meta operations (they manage filesystem
    // projects/templates, not the runtime protocol), so they are implemented
    // here instead of the engine RPC DTO chain -- consistent with the
    // task-book principle that Editor-internal RPC may evolve freely.
    // ---------------------------------------------------------------------

    // Templates root: tools/project_templates under the repo root. Paths are
    // confined to <cwd>/tools/project_templates (no ".." / absolute escapes).
    const auto templatesRoot = []() -> fs::path {
        fs::path cwd = fs::current_path();
        return cwd / "tools" / "project_templates";
    };

    const auto confineTemplatePath = [&](const std::string& p) -> fs::path {
        std::string norm = p;
        for (auto& ch : norm) if (ch == char(92)) ch = '/';
        if (norm.find("..") != std::string::npos) return {};
        if (!norm.empty() && norm[0] == '/') return {};
        if (norm.find(':') != std::string::npos) return {};   // drive/scheme
        return templatesRoot() / norm;
    };

    svr.Get("/api/project/templates", [&](const httplib::Request&, httplib::Response& res) {
        nlohmann::json out = nlohmann::json::array();
        // Read manifest.json when present; otherwise enumerate directories.
        std::ifstream mf(templatesRoot() / "manifest.json");
        if (mf) {
            try {
                auto m = nlohmann::json::parse(mf);
                if (m.contains("templates") && m["templates"].is_array())
                    out = m["templates"];
            } catch (const std::exception&) { out = nlohmann::json::array(); }
        }
        if (out.empty()) {
            // Fallback: enumerate directories under templates root.
            std::error_code ec;
            for (const auto& e : fs::directory_iterator(templatesRoot(), ec)) {
                if (ec || !e.is_directory()) continue;
                out.push_back({ {"id", e.path().filename().string()},
                                {"name", e.path().filename().string()},
                                {"description", ""},
                                {"dir", e.path().filename().string()} });
            }
        }
        res.set_content(out.dump(), "application/json");
    });

    svr.Get("/api/project/list", [&](const httplib::Request&, httplib::Response& res) {
        // Discover projects under ./projects/ (created by this manager). Each
        // is a directory with story.ks (or entry.lua) and assets/. Hard catch
        // keeps a malformed entry from becoming a 500 (Sprint 2).
        nlohmann::json out = nlohmann::json::array();
        try {
            fs::path projectsRoot = fs::current_path() / "projects";
            std::error_code ec;
            if (!fs::exists(projectsRoot, ec)) {
                std::error_code createEc;
                fs::create_directories(projectsRoot, createEc);
                res.set_content(out.dump(), "application/json");
                return;
            }
            for (const auto& e : fs::directory_iterator(projectsRoot, ec)) {
                if (ec) break;
                std::error_code de;
                if (!e.is_directory(de) || de) continue;
                const std::string name = e.path().filename().string();
                const bool hasStory = fs::exists(e.path() / "story.ks", de)
                                    || fs::exists(e.path() / "entry.lua", de);
                if (!hasStory) continue;
                std::string modified;
                std::error_code le;
                auto ft = fs::last_write_time(e.path(), le);
                if (!le) {
                    modified = std::to_string(
                        static_cast<long long>(ft.time_since_epoch().count()));
                }
                out.push_back({ {"path", std::string("projects/") + name},
                                {"name", name},
                                {"template", ""},
                                {"modified", modified} });
            }
        } catch (...) {
            out = nlohmann::json::array();
        }
        std::string body;
        try { body = out.dump(); } catch (...) { body = "[]"; }
        res.set_content(body, "application/json");
    });

    svr.Post("/api/project/create", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            const std::string templateId = body.value("template", "basic");
            const std::string name = body.value("name", std::string());
            if (name.empty()) {
                res.set_content("{\"error\":\"Missing project name\"}", "application/json");
                res.status = 400;
                return;
            }
            if (templateId != "blank" && templateId != "basic" &&
                templateId != "live2d" && templateId != "kag3" &&
                templateId != "showcase" && !templateId.empty()) {
                res.set_content("{\"error\":\"Unknown template\"}", "application/json");
                res.status = 400;
                return;
            }
            // Sanitize project name: allow [A-Za-z0-9_-], reject separators.
            for (char ch : name) {
                if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-')) {
                    res.set_content("{\"error\":\"Invalid project name\"}", "application/json");
                    res.status = 400;
                    return;
                }
            }
            fs::path projectsRoot = fs::current_path() / "projects";
            std::error_code ec;
            fs::create_directories(projectsRoot, ec);

            fs::path dest = projectsRoot / name;
            if (fs::exists(dest, ec)) {
                res.set_content("{\"error\":\"Project already exists\"}", "application/json");
                res.status = 409;
                return;
            }
            // Copy template dir (default "basic") into the project.
            const std::string dir = templateId.empty() ? "basic" : templateId;
            fs::path src = confineTemplatePath(dir);
            if (src.empty() || !fs::exists(src, ec)) {
                res.set_content("{\"error\":\"Template not found\"}", "application/json");
                res.status = 400;
                return;
            }
            std::error_code copyEc;
            fs::copy(src, dest, fs::copy_options::recursive
                                        | fs::copy_options::overwrite_existing,
                     copyEc);
            if (copyEc) {
                res.set_content("{\"error\":\"Failed to copy template\"}", "application/json");
                res.status = 500;
                return;
            }
            // Adjust entry.lua story path is unnecessary (it already probes
            // several relative locations).
            res.set_content(dumpJson({{"ok", true}, {"path", dest.string()}}),
                            "application/json");
        } catch (const std::exception&) {
            res.set_content("{\"error\":\"Invalid JSON body\"}", "application/json");
            res.status = 400;
        }
    });

    svr.Post("/api/project/duplicate", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            const std::string srcName = body.value("srcPath", std::string());
            const std::string newName = body.value("name", std::string());
            if (srcName.empty() || newName.empty()) {
                res.set_content("{\"error\":\"Missing srcPath/name\"}", "application/json");
                res.status = 400;
                return;
            }
            for (char ch : newName) {
                if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-')) {
                    res.set_content("{\"error\":\"Invalid project name\"}", "application/json");
                    res.status = 400;
                    return;
                }
            }
            fs::path projectsRoot = fs::current_path() / "projects";
            fs::path src = projectsRoot / srcName;
            fs::path dest = projectsRoot / newName;
            std::error_code ec;
            if (!fs::exists(src, ec) || !fs::is_directory(src, ec)) {
                res.set_content("{\"error\":\"Source project not found\"}", "application/json");
                res.status = 404;
                return;
            }
            if (fs::exists(dest, ec)) {
                res.set_content("{\"error\":\"Project already exists\"}", "application/json");
                res.status = 409;
                return;
            }
            std::error_code copyEc;
            fs::copy(src, dest, fs::copy_options::recursive
                                        | fs::copy_options::overwrite_existing,
                     copyEc);
            if (copyEc) {
                res.set_content("{\"error\":\"Failed to duplicate\"}", "application/json");
                res.status = 500;
                return;
            }
            res.set_content(dumpJson({{"ok", true}, {"path", dest.string()}}),
                            "application/json");
        } catch (const std::exception&) {
            res.set_content("{\"error\":\"Invalid JSON body\"}", "application/json");
            res.status = 400;
        }
    });

    svr.Post("/api/project/import", [&](const httplib::Request& req, httplib::Response& res) {
        // Import semantics differ from duplicate on purpose: srcPath may be
        // ANY on-disk directory (absolute or relative -- that is the whole
        // point of importing an existing game), while only the destination
        // NAME goes through the sanitizer and lands under ./projects/.
        try {
            auto body = nlohmann::json::parse(req.body);
            const std::string srcStr = body.value("srcPath", std::string());
            const std::string newName = body.value("name", std::string());
            if (srcStr.empty() || newName.empty()) {
                res.set_content("{\"error\":\"Missing srcPath/name\"}", "application/json");
                res.status = 400;
                return;
            }
            for (char ch : newName) {
                if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-')) {
                    res.set_content("{\"error\":\"Invalid project name\"}", "application/json");
                    res.status = 400;
                    return;
                }
            }
            std::error_code ec;
            const fs::path src(srcStr);
            if (!fs::exists(src, ec) || !fs::is_directory(src, ec)) {
                res.set_content("{\"error\":\"Source directory not found\"}", "application/json");
                res.status = 404;
                return;
            }
            // A Caesura project carries a story entry point (story.ks
            // preferred, entry.lua fallback) -- mirrors /api/project/list
            // discovery so imported projects are listable afterwards.
            if (!fs::exists(src / "story.ks", ec)
                && !fs::exists(src / "entry.lua", ec)) {
                res.set_content(
                    "{\"error\":\"Not a Caesura project (missing story.ks/entry.lua)\"}",
                    "application/json");
                res.status = 400;
                return;
            }
            fs::path projectsRoot = fs::current_path() / "projects";
            std::error_code mkEc;
            fs::create_directories(projectsRoot, mkEc);
            fs::path dest = projectsRoot / newName;
            if (fs::exists(dest, ec)) {
                res.set_content("{\"error\":\"Project already exists\"}", "application/json");
                res.status = 409;
                return;
            }
            std::error_code copyEc;
            fs::copy(src, dest, fs::copy_options::recursive
                                        | fs::copy_options::overwrite_existing,
                     copyEc);
            if (copyEc) {
                res.set_content("{\"error\":\"Failed to import project\"}", "application/json");
                res.status = 500;
                return;
            }
            res.set_content(dumpJson({{"ok", true}, {"path", dest.string()}}),
                            "application/json");
        } catch (const std::exception&) {
            res.set_content("{\"error\":\"Invalid JSON body\"}", "application/json");
            res.status = 400;
        }
    });

    // ---------------------------------------------------------------------
    // Start listening
    // ---------------------------------------------------------------------
    printf("[EditorServer] Listening on port %d...\n", port);
    if (!svr.listen_after_bind()) {
        DEBUG_ERR(SubSys::Platform, ErrCode::Ok,
                  "[EditorServer] Failed to listen on port %d", port);
    }
    m_running = false;
}

} // namespace Caesura
