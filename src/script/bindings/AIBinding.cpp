// AIBinding.cpp - LLM query binding (see AIBinding.h for the API surface).
// Uses httplib (header-only, outbound HTTP) + nlohmann_json, both exposed
// to every module via CaesarBuildOptions. Async requests run the HTTP call
// on a JobSystem worker; the Lua callback fires on the main thread from
// pollMainThreadJobs (Lua is never touched off the owner thread).
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "AIBinding.h"
#include <httplib.h>
#include <nlohmann_json.hpp>
#include "../../job/api/IJobSystem.h"
#include "../../di/BackendRegistry.h"
#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

namespace Caesura {

namespace {

// Per-request state shared between the worker and the main-thread callback.
struct AiRequest {
    std::string result;
    std::string error;
    bool done = false;
};

std::atomic<uint64_t> g_nextRequestId{1};
std::atomic<bool> g_cancelFlag{false};

// ---- config read (config.ai.*) -------------------------------------------

struct AiConfig {
    std::string endpoint;
    std::string model;
    std::string apiKey;
    std::string system;
    int timeoutMs = 15000;
};

AiConfig readConfig(lua_State* L) {
    AiConfig cfg;
    lua_getglobal(L, "config");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "ai");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "endpoint");
            if (lua_isstring(L, -1)) cfg.endpoint = lua_tostring(L, -1);
            lua_pop(L, 1);
            lua_getfield(L, -1, "model");
            if (lua_isstring(L, -1)) cfg.model = lua_tostring(L, -1);
            lua_pop(L, 1);
            lua_getfield(L, -1, "api_key");
            if (lua_isstring(L, -1)) cfg.apiKey = lua_tostring(L, -1);
            lua_pop(L, 1);
            lua_getfield(L, -1, "system");
            if (lua_isstring(L, -1)) cfg.system = lua_tostring(L, -1);
            lua_pop(L, 1);
            lua_getfield(L, -1, "timeout_ms");
            if (lua_isnumber(L, -1)) cfg.timeoutMs = (int)lua_tointeger(L, -1);
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return cfg;
}

// opts table (optional 2nd arg of query/query_async) may override
// endpoint/model/system/timeout_ms.
void applyOpts(lua_State* L, int idx, AiConfig& cfg) {
    if (!lua_istable(L, idx)) return;
    lua_getfield(L, idx, "endpoint");
    if (lua_isstring(L, -1)) cfg.endpoint = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, idx, "model");
    if (lua_isstring(L, -1)) cfg.model = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, idx, "system");
    if (lua_isstring(L, -1)) cfg.system = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, idx, "timeout_ms");
    if (lua_isnumber(L, -1)) cfg.timeoutMs = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
}

// ---- HTTP query -----------------------------------------------------------

// Do the actual HTTP round trip. Thread-safe (no Lua access).
void doQuery(const std::string& prompt, const AiConfig& cfg,
             std::string& outText, std::string& outErr) {
    if (cfg.endpoint.empty()) {
        outErr = "no-endpoint";
        return;
    }
    // Split endpoint into scheme://host:port and path prefix.
    std::string host = cfg.endpoint;
    std::string pathPrefix;
    const std::string scheme = "http://";
    if (host.rfind(scheme, 0) == 0) {
        host = host.substr(scheme.size());
    }
    auto slash = host.find('/');
    if (slash != std::string::npos) {
        pathPrefix = host.substr(slash);
        host = host.substr(0, slash);
    }
    int port = 80;
    auto colon = host.rfind(':');
    if (colon != std::string::npos) {
        port = std::atoi(host.substr(colon + 1).c_str());
        host = host.substr(0, colon);
    }
    if (host.empty()) {
        outErr = "invalid-endpoint";
        return;
    }

    httplib::Client cli(host, port);
    cli.set_connection_timeout(2, 0);
    cli.set_read_timeout(cfg.timeoutMs / 1000,
                         (cfg.timeoutMs % 1000) * 1000);
    cli.set_write_timeout(5, 0);

    // OpenAI-compatible when the path prefix contains "/v1" (with or
    // without trailing slash: ".../v1", ".../v1/", ".../api/v1").
    const bool openai = cfg.endpoint.find("/v1") != std::string::npos;

    nlohmann::json body;
    std::string path;
    if (openai) {
        path = pathPrefix + "/chat/completions";
        body["model"] = cfg.model.empty() ? "gpt-4o-mini" : cfg.model;
        nlohmann::json messages = nlohmann::json::array();
        if (!cfg.system.empty()) {
            messages.push_back(
                {{"role", "system"}, {"content", cfg.system}});
        }
        messages.push_back({{"role", "user"}, {"content", prompt}});
        body["messages"] = messages;
        body["stream"] = false;
    } else {
        path = pathPrefix + "/api/generate";
        body["model"] = cfg.model.empty() ? "llama3" : cfg.model;
        body["prompt"] = prompt;
        if (!cfg.system.empty()) body["system"] = cfg.system;
        body["stream"] = false;
    }

    auto req = std::make_shared<httplib::Request>();
    req->method = "POST";
    req->path = path;
    req->set_header("Content-Type", "application/json");
    if (!cfg.apiKey.empty()) {
        req->set_header("Authorization", "Bearer " + cfg.apiKey);
    }
    req->body = body.dump();

    auto res = cli.send(*req);
    if (!res) {
        outErr = "connection-failed";
        return;
    }
    if (res->status != 200) {
        outErr = "http-" + std::to_string(res->status);
        return;
    }
    try {
        auto j = nlohmann::json::parse(res->body);
        if (openai) {
            const auto& choices = j["choices"];
            if (choices.is_array() && !choices.empty()) {
                outText = choices[0]["message"]["content"].get<std::string>();
            }
        } else {
            outText = j.value("response", std::string());
        }
    } catch (const std::exception& e) {
        outErr = std::string("parse-error: ") + e.what();
    }
    if (outText.empty() && outErr.empty()) {
        outErr = "empty-response";
    }
}

// ---- Lua functions --------------------------------------------------------

static int lua_AI_available(lua_State* L) {
    AiConfig cfg = readConfig(L);
    lua_pushboolean(L, !cfg.endpoint.empty());
    return 1;
}

static int lua_AI_query(lua_State* L) {
    const char* prompt = luaL_checkstring(L, 1);
    AiConfig cfg = readConfig(L);
    if (lua_gettop(L) >= 2) applyOpts(L, 2, cfg);

    std::string text, err;
    doQuery(prompt, cfg, text, err);
    if (!err.empty() || text.empty()) {
        lua_pushnil(L);
        lua_pushstring(L, err.empty() ? "empty-response" : err.c_str());
        return 2;
    }
    lua_pushstring(L, text.c_str());
    return 1;
}

static int lua_AI_query_async(lua_State* L) {
    const char* prompt = luaL_checkstring(L, 1);
    AiConfig cfg = readConfig(L);
    if (lua_gettop(L) >= 2 && lua_istable(L, 2)) applyOpts(L, 2, cfg);
    // Callback at arg 3, or arg 2 when opts omitted.
    int cbIdx = 3;
    if (lua_gettop(L) >= 2 && lua_isfunction(L, 2)) cbIdx = 2;
    if (!lua_isfunction(L, cbIdx)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    auto* jobs = BackendRegistry::instance().getJobSystem();
    if (!jobs) {
        lua_pushboolean(L, 0);
        return 1;
    }

    // Store the callback: global _AI_CALLBACKS[id] = ref.
    const uint64_t id = g_nextRequestId.fetch_add(1);
    lua_getglobal(L, "_AI_CALLBACKS");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "_AI_CALLBACKS");
    }
    lua_insert(L, -2);  // [callbacks, callback]
    int cbRef = luaL_ref(L, LUA_REGISTRYINDEX);  // pops callback
    lua_pushinteger(L, (lua_Integer)id);
    lua_pushinteger(L, cbRef);
    lua_settable(L, -3);
    lua_pop(L, 1);  // callbacks table

    auto request = std::make_shared<AiRequest>();
    std::string promptCopy = prompt;
    AiConfig cfgCopy = cfg;
    jobs->submit(
        [request, promptCopy, cfgCopy]() {
            doQuery(promptCopy, cfgCopy, request->result, request->error);
            if (g_cancelFlag.load()) {
                request->error = "cancelled";
            }
            request->done = true;
        },
        JobPriority::Normal,
        [L, id, request]() {
            // Main thread: invoke the stored callback with (text, err).
            lua_getglobal(L, "_AI_CALLBACKS");
            if (!lua_istable(L, -1)) {
                lua_pop(L, 1);
                return;
            }
            lua_pushinteger(L, (lua_Integer)id);
            lua_gettable(L, -2);  // callback ref (number) or nil
            int cbRef = lua_isnil(L, -1) ? LUA_NOREF
                                         : (int)lua_tointeger(L, -1);
            lua_pop(L, 1);
            lua_pushnil(L);  // remove the callback entry
            lua_setfield(L, -2, std::to_string(id).c_str());
            lua_pop(L, 1);  // callbacks table

            if (cbRef != LUA_NOREF) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, cbRef);
                luaL_unref(L, LUA_REGISTRYINDEX, cbRef);
                if (lua_isfunction(L, -1)) {
                    if (!request->error.empty() || request->result.empty()) {
                        lua_pushnil(L);
                        lua_pushstring(
                            L, request->error.empty() ? "empty-response"
                                                      : request->error.c_str());
                    } else {
                        lua_pushstring(L, request->result.c_str());
                        lua_pushnil(L);
                    }
                    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                        const char* err = lua_tostring(L, -1);
                        fprintf(stderr, "[AI] callback error: %s\n",
                                err ? err : "(unknown)");
                        lua_pop(L, 1);
                    }
                } else {
                    lua_pop(L, 1);
                }
            }
        });
    lua_pushboolean(L, 1);
    return 1;
}

static int lua_AI_cancel(lua_State* L) {
    g_cancelFlag.store(true);
    // Drop all stored callbacks so they never fire after cancel.
    lua_getglobal(L, "_AI_CALLBACKS");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            int cbRef = (int)lua_tointeger(L, -1);
            luaL_unref(L, LUA_REGISTRYINDEX, cbRef);
            lua_pop(L, 1);
        }
        lua_newtable(L);
        lua_setglobal(L, "_AI_CALLBACKS");
    }
    lua_pop(L, 1);
    lua_pushboolean(L, 1);
    return 1;
}

const luaL_Reg ai_functions[] = {
    {"available", lua_AI_available},
    {"query", lua_AI_query},
    {"query_async", lua_AI_query_async},
    {"cancel", lua_AI_cancel},
    {nullptr, nullptr},
};

} // namespace

void registerAIBinding(lua_State* L) {
    luaL_newlib(L, ai_functions);
    lua_setglobal(L, "AI");
    printf("[Lua] AI module registered (LLM query binding).\n");
}

} // namespace Caesura
