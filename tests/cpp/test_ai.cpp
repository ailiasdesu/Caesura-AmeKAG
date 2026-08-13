// test_ai.cpp - AI query binding tests (LLM endpoint, sync + async).
// Spins a local httplib server that emulates OpenAI-compatible and Ollama
// endpoints; verifies request/response formats, and graceful failure when
// no service is listening.
#include "doctest.h"
#include <httplib.h>
#include <nlohmann_json.hpp>
#include <thread>
#include <atomic>

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
TEST_CASE("AI query against real Ollama (skipped when unreachable)") {
    // Real end-to-end: requires a local Ollama server on 127.0.0.1:11434.
    // Follows the test_audio pattern -- clean skip on machines without
    // Ollama (CI), real verification where it is installed. Uses an EMPTY
    // model so the binding exercises its auto-discovery (GET /api/tags)
    // against the live server; discovery failure would fall back to
    // "llama3" and surface as http-404 here.
    httplib::Client probe("127.0.0.1", 11434);
    probe.set_connection_timeout(1, 0);
    probe.set_read_timeout(2, 0);
    auto tags = probe.Get("/api/tags");
    if (!tags || tags->status != 200) {
        MESSAGE("Ollama unreachable (127.0.0.1:11434), skipping real-AI test");
        return;
    }
    std::string model;
    try {
        auto j = nlohmann::json::parse(tags->body);
        const auto& models = j["models"];
        if (models.is_array() && !models.empty()) {
            model = models[0].value("name", std::string());
        }
    } catch (const std::exception&) {}
    REQUIRE(!model.empty());  // server reachable but no models pulled

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dostring(L,
        "config = { ai = { endpoint = 'http://127.0.0.1:11434',"
        " model = '', timeout_ms = 120000 } }");
    Caesura::registerAIBinding(L);

    lua_getglobal(L, "AI");
    lua_getfield(L, -1, "query");
    lua_pushstring(L, "Reply with the single word: hello");
    REQUIRE(lua_pcall(L, 1, 2, 0) == LUA_OK);
    if (lua_isstring(L, -2)) {
        std::string reply = lua_tostring(L, -2);
        CHECK(!reply.empty());
        MESSAGE("Real Ollama reply (auto-discovered model " << model
                << "): " << reply.substr(0, 80));
    } else {
        FAIL("real ollama query returned nil: " << lua_tostring(L, -1));
    }
    lua_close(L);
}
