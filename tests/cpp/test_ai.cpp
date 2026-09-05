// test_ai.cpp - AI query binding tests (LLM endpoint, sync + async).
// Spins a local httplib server that emulates OpenAI-compatible and Ollama
// endpoints; verifies request/response formats, and graceful failure when
// no service is listening.
#include "doctest.h"
#include <httplib.h>
#include <nlohmann_json.hpp>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include "script/vm/LuaManager.h"

// Include the binding sources directly so the tests exercise the same code
// the engine links (no separate test target for bindings).
#include "../src/script/bindings/AIBinding.h"
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

TEST_CASE("AI query against OpenAI-compatible mock") {
    // Mock OpenAI /v1/chat/completions server.
    httplib::Server srv;
    std::atomic<bool> gotRequest{false};
    std::string gotBody;
    srv.Post("/v1/chat/completions", [&](const httplib::Request& req,
                                         httplib::Response& res) {
        gotRequest = true;
        gotBody = req.body;
        res.set_content(
            R"({"choices":[{"message":{"role":"assistant","content":"Hello from mock AI"}}]})",
            "application/json");
    });
    // This httplib version does not expose the bound port; try a small
    // range of fixed ports until one binds.
    int port = 0;
    for (int p = 18921; p <= 18940 && port == 0; ++p) {
        if (srv.bind_to_port("127.0.0.1", p)) port = p;
    }
    REQUIRE(port != 0);
    std::thread t([&]() { srv.listen_after_bind(); });

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    // config.ai endpoint for the binding to read.
    luaL_dostring(L, ("config = { ai = { endpoint = "
        "'http://127.0.0.1:" + std::to_string(port)
        + "/v1', model = 'test-model', timeout_ms = 3000 } }").c_str());
    Caesura::registerAIBinding(L);

    lua_getglobal(L, "AI");
    REQUIRE(lua_istable(L, -1));
    lua_getfield(L, -1, "query");
    REQUIRE(lua_isfunction(L, -1));
    lua_pushstring(L, "hello");
    REQUIRE(lua_pcall(L, 1, 2, 0) == LUA_OK);
    if (lua_isstring(L, -2)) {
        CHECK(std::string(lua_tostring(L, -2)) == "Hello from mock AI");
    } else {
        FAIL("query returned nil: " << lua_tostring(L, -1));
    }
    CHECK(gotRequest == true);
    CHECK(gotBody.find("\"model\":\"test-model\"") != std::string::npos);
    CHECK(gotBody.find("\"role\":\"user\"") != std::string::npos);

    lua_close(L);
    srv.stop();
    t.join();
}

TEST_CASE("AI query against Ollama-style mock") {
    httplib::Server srv;
    srv.Post("/api/generate", [&](const httplib::Request& req,
                                  httplib::Response& res) {
        res.set_content(R"({"response":"Ollama says hi","done":true})",
                        "application/json");
    });
    int port = 0;
    for (int p = 18941; p <= 18960 && port == 0; ++p) {
        if (srv.bind_to_port("127.0.0.1", p)) port = p;
    }
    REQUIRE(port != 0);
    std::thread t([&]() { srv.listen_after_bind(); });

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dostring(L, ("config = { ai = { endpoint = "
        "'http://127.0.0.1:" + std::to_string(port) + "', timeout_ms = 3000 } }").c_str());
    Caesura::registerAIBinding(L);

    lua_getglobal(L, "AI");
    lua_getfield(L, -1, "query");
    lua_pushstring(L, "hi");
    REQUIRE(lua_pcall(L, 1, 2, 0) == LUA_OK);
    if (lua_isstring(L, -2)) {
        CHECK(std::string(lua_tostring(L, -2)) == "Ollama says hi");
    } else {
        FAIL("ollama query returned nil: " << lua_tostring(L, -1));
    }

    lua_close(L);
    srv.stop();
    t.join();
}

TEST_CASE("AI query fails gracefully with no service") {
    // Use a port range we just freed in the previous tests (small race,
    // acceptable for the failure-path assertion).
    int port = 18999;

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dostring(L, ("config = { ai = { endpoint = "
        "'http://127.0.0.1:" + std::to_string(port) + "', timeout_ms = 500 } }").c_str());
    Caesura::registerAIBinding(L);

    lua_getglobal(L, "AI");
    lua_getfield(L, -1, "query");
    lua_pushstring(L, "ping");
    REQUIRE(lua_pcall(L, 1, 2, 0) == LUA_OK);
    CHECK(lua_isnil(L, -2));  // nil result
    CHECK(lua_isstring(L, -1));  // error string
    lua_close(L);
}

TEST_CASE("AI.available false when endpoint unset") {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dostring(L, "config = { ai = { endpoint = '' } }");
    Caesura::registerAIBinding(L);

    lua_getglobal(L, "AI");
    lua_getfield(L, -1, "available");
    REQUIRE(lua_pcall(L, 0, 1, 0) == LUA_OK);
    CHECK(lua_toboolean(L, -1) == false);
    lua_close(L);
}
// Real-service coverage remains in CaesuraHeadlessAiSmoke (optional SKIP 77
// when no local Ollama/model is available). This required C++ test verifies
// HTTP discovery and the native Lua binding contract, not model quality.
TEST_CASE("AI query auto-discovers an Ollama model over loopback HTTP") {
    httplib::Server server;
    std::atomic<int> discoveries{0};
    std::mutex requestsMutex;
    std::vector<std::string> requests;
    server.Get("/api/tags", [&](const httplib::Request&, httplib::Response& res) {
        ++discoveries;
        res.set_content(R"({"models":[{"name":"fixture-model:latest"}]})",
                        "application/json");
    });
    server.Post("/api/generate", [&](const httplib::Request& req, httplib::Response& res) {
        {
            std::lock_guard<std::mutex> lock(requestsMutex);
            requests.push_back(req.body);
        }
        res.set_content(R"({"response":"deterministic loopback reply","done":true})",
                        "application/json");
    });
    const int port = server.bind_to_any_port("127.0.0.1");
    REQUIRE(port > 0);
    // Stop/join even when a REQUIRE throws; never leave a joinable thread.
    struct Listener {
        httplib::Server& server;
        std::thread worker;
        explicit Listener(httplib::Server& value)
            : server(value), worker([&value]() { value.listen_after_bind(); }) {
            server.wait_until_ready();
        }
        ~Listener() {
            server.stop();
            if (worker.joinable()) worker.join();
        }
    } listener(server);

    Caesura::LuaManager lua;
    struct LuaShutdown {
        Caesura::LuaManager& manager;
        ~LuaShutdown() { manager.shutdown(); }
    } cleanup{lua};
    REQUIRE(lua.init());
    lua_State* L = lua.state();
    const char* code = R"lua(
        local endpoint = ...
        config = {ai = {endpoint = endpoint, model = '', timeout_ms = 3000}}
        assert(AI.available(), 'native AI binding must be available')
        local reply, err = AI.query('discover this model', {system = 'fixture system'})
        assert(reply == 'deterministic loopback reply', 'native query response')
        assert(err == nil, 'successful query must not return an error')
    )lua";
    REQUIRE(luaL_loadstring(L, code) == LUA_OK);
    const std::string endpoint = "http://127.0.0.1:" + std::to_string(port);
    lua_pushlstring(L, endpoint.data(), endpoint.size());
    const int result = lua_pcall(L, 1, 0, 0);
    INFO("Lua error: " << (result != LUA_OK && lua_tostring(L, -1) ? lua_tostring(L, -1) : "none"));
    REQUIRE(result == LUA_OK);
    CHECK(discoveries.load() == 1);
    std::lock_guard<std::mutex> lock(requestsMutex);
    REQUIRE(requests.size() == 1);
    const auto body = nlohmann::json::parse(requests.front());
    CHECK(body.at("model") == "fixture-model:latest");
    CHECK(body.at("prompt") == "discover this model");
    CHECK(body.at("system") == "fixture system");
    CHECK(body.at("stream") == false);
}
