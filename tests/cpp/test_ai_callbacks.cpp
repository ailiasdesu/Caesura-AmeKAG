#include "doctest.h"
#include "di/BackendRegistry.h"
#include "job/api/IJobSystem.h"
#include "script/bindings/AIBinding.h"
#include "mocks/NullJobSystem.h"
#include <httplib.h>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace {

// Execute the real HTTP worker and its completion separately, so cancellation
// on either side of worker completion never depends on timing or sleeps.
class ControlledAiJobs final : public Caesura::IJobSystem {
public:
    struct Pending {
        Caesura::JobFn work;
        Caesura::MainThreadFn complete;
    };
    ControlledAiJobs()
        : previous(Caesura::BackendRegistry::instance().getJobSystem()) {
        Caesura::BackendRegistry::instance().setJobSystem(this);
    }
    ~ControlledAiJobs() override {
        Caesura::BackendRegistry::instance().setJobSystem(previous);
    }
    void init() override { running = true; }
    void shutdown() override { running = false; }
    uint64_t submit(Caesura::JobFn work, Caesura::JobPriority,
                    Caesura::MainThreadFn complete) override {
        if (!running || !work) return 0;
        pending.push_back({std::move(work), std::move(complete)});
        return pending.size();
    }
    void pollMainThreadJobs() override {}
    void waitIdle() override {}
    int workerCount() const override { return 0; }
    int pendingJobs() const override { return static_cast<int>(pending.size()); }
    bool isRunning() const override { return running; }
    void work(size_t index) {
        REQUIRE(index < pending.size());
        const auto callback = pending[index].work;
        callback();
    }
    void complete(size_t index) {
        REQUIRE(index < pending.size());
        // Copy before calling: callback code can append another job.
        const auto callback = pending[index].complete;
        REQUIRE(static_cast<bool>(callback));
        callback();
    }

    std::vector<Pending> pending;
    bool running = true;
    Caesura::IJobSystem* previous;
};

using LuaState = std::unique_ptr<lua_State, decltype(&lua_close)>;

LuaState makeAiLua() {
    LuaState state(luaL_newstate(), lua_close);
    REQUIRE(state != nullptr);
    luaL_openlibs(state.get());
    Caesura::registerAIBinding(state.get());
    return state;
}

void run(lua_State* state, const char* source) {
    const int top = lua_gettop(state);
    int result = luaL_loadstring(state, source);
    if (result == LUA_OK) result = lua_pcall(state, 0, 0, 0);
    const char* message = result == LUA_OK ? nullptr : lua_tostring(state, -1);
    const std::string error = message ? message : "none";
    INFO("Lua error: " << error);
    INFO("Lua source: " << source);
    REQUIRE(result == LUA_OK);
    REQUIRE(lua_gettop(state) == top);
}

class AiLoopback {
public:
    AiLoopback() {
        server.Post("/v1/chat/completions", [](const httplib::Request&,
                                                httplib::Response& response) {
            response.set_content(
                R"({"choices":[{"message":{"content":"fixture reply"}}]})",
                "application/json");
        });
        port = server.bind_to_any_port("127.0.0.1");
        REQUIRE(port > 0);
        listener = std::thread([this]() { server.listen_after_bind(); });
        server.wait_until_ready();
    }
    ~AiLoopback() {
        server.stop();
        if (listener.joinable()) listener.join();
    }
    void configure(lua_State* state) const {
        const auto source = "config = {ai = {endpoint = 'http://127.0.0.1:"
            + std::to_string(port) + "/v1', model = 'fixture', timeout_ms = 3000}}";
        run(state, source.c_str());
    }

private:
    httplib::Server server;
    std::thread listener;
    int port = 0;
};

int retainSentinel(lua_State* state) {
    lua_newtable(state);
    lua_pushstring(state, "unrelated registry owner");
    lua_setfield(state, -2, "value");
    return luaL_ref(state, LUA_REGISTRYINDEX);
}

void checkSentinel(lua_State* state, int reference) {
    lua_rawgeti(state, LUA_REGISTRYINDEX, reference);
    CHECK(lua_istable(state, -1));
    if (lua_istable(state, -1)) {
        lua_getfield(state, -1, "value");
        const char* value = lua_tostring(state, -1);
        CHECK(value != nullptr);
        if (value) CHECK(std::string(value) == "unrelated registry owner");
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
}

} // namespace

TEST_CASE("AI callback completion releases its reference exactly once") {
    ControlledAiJobs jobs;
    auto lua = makeAiLua();
    run(lua.get(), R"lua(
        calls = 0
        assert(AI.query_async('no endpoint', function(text, err)
            calls = calls + 1
            assert(text == nil and err == 'no-endpoint')
        end))
    )lua");
    jobs.work(0);
    jobs.complete(0);
    run(lua.get(), "assert(calls == 1)");

    const int sentinel = retainSentinel(lua.get());
    SUBCASE("cancel after completion leaves a reused registry reference alive") {
        run(lua.get(), "assert(AI.cancel())");
    }
    SUBCASE("redelivered completion leaves a reused registry reference alive") {
        jobs.complete(0);
    }
    checkSentinel(lua.get(), sentinel);
    run(lua.get(), "assert(calls == 1)");
    luaL_unref(lua.get(), LUA_REGISTRYINDEX, sentinel);
}

TEST_CASE("AI callback can cancel and enqueue a successor exactly once") {
    ControlledAiJobs jobs;
    AiLoopback server;
    auto lua = makeAiLua();
    server.configure(lua.get());
    run(lua.get(), R"lua(
        first, successor, reply, failure = 0, 0, nil, nil
        assert(AI.query_async('first', function()
            first = first + 1
            assert(AI.cancel())
            assert(AI.query_async('successor', function(text, err)
                successor = successor + 1
                reply, failure = text, err
            end))
        end))
    )lua");
    jobs.work(0);
    jobs.complete(0);
    REQUIRE(jobs.pending.size() == 2);
    jobs.work(1);
    jobs.complete(1);
    run(lua.get(), R"lua(
        assert(first == 1 and successor == 1, 'both callbacks must run once')
        assert(reply == 'fixture reply' and failure == nil,
               'successor must keep its successful response: ' .. tostring(failure))
    )lua");
    const int sentinel = retainSentinel(lua.get());
    jobs.complete(0);
    jobs.complete(1);
    checkSentinel(lua.get(), sentinel);
    run(lua.get(), "assert(first == 1 and successor == 1)");
    luaL_unref(lua.get(), LUA_REGISTRYINDEX, sentinel);
}

TEST_CASE("AI cancel suppresses stale success and error completions") {
    ControlledAiJobs jobs;
    AiLoopback server;
    auto lua = makeAiLua();
    bool finishBeforeCancel = false;
    SUBCASE("successful worker completes before cancel") {
        server.configure(lua.get());
        finishBeforeCancel = true;
    }
    SUBCASE("successful worker completes after cancel") {
        server.configure(lua.get());
    }
    SUBCASE("failed worker completes before cancel") {
        finishBeforeCancel = true;
    }
    SUBCASE("failed worker completes after cancel") {}
    run(lua.get(), R"lua(
        obsolete = 0
        assert(AI.query_async('obsolete', function() obsolete = obsolete + 1 end))
    )lua");
    if (finishBeforeCancel) jobs.work(0);
    run(lua.get(), "assert(AI.cancel())");
    server.configure(lua.get());
    run(lua.get(), R"lua(
        current, reply, failure = 0, nil, nil
        assert(AI.query_async('current', function(text, err)
            current = current + 1
            reply, failure = text, err
        end))
    )lua");
    if (!finishBeforeCancel) jobs.work(0);
    jobs.complete(0);
    run(lua.get(), "assert(obsolete == 0 and current == 0)");
    jobs.work(1);
    jobs.complete(1);
    run(lua.get(), R"lua(
        assert(obsolete == 0 and current == 1, 'cancel must suppress only obsolete callback')
        assert(reply == 'fixture reply' and failure == nil,
               'new request must keep its successful response: ' .. tostring(failure))
    )lua");
}

TEST_CASE("AI cancellation in one VM leaves another VM request unchanged") {
    ControlledAiJobs jobs;
    AiLoopback server;
    auto first = makeAiLua();
    auto second = makeAiLua();
    server.configure(second.get());
    run(second.get(), R"lua(
        calls, reply, failure = 0, nil, nil
        assert(AI.query_async('another VM', function(text, err)
            calls = calls + 1
            reply, failure = text, err
        end))
    )lua");
    run(first.get(), "assert(AI.cancel())");
    jobs.work(0);
    jobs.complete(0);
    run(second.get(), R"lua(
        assert(calls == 1, 'other VM callback must run once')
        assert(reply == 'fixture reply' and failure == nil,
               'other VM must keep its successful response: ' .. tostring(failure))
    )lua");
}

TEST_CASE("AI callback submitted by a coroutine executes on its main Lua thread") {
    ControlledAiJobs jobs;
    Caesura::NullJobSystem inlineJobs;
    inlineJobs.init();
    bool inlineCompletion = false;
    SUBCASE("queued completion after submitter collection") {}
    SUBCASE("inline completion") { inlineCompletion = true; }
    auto lua = makeAiLua();
    struct RestoreJobBinding {
        Caesura::IJobSystem* previous;
        ~RestoreJobBinding() { Caesura::BackendRegistry::instance().setJobSystem(previous); }
    } restore{Caesura::BackendRegistry::instance().getJobSystem()};
    if (inlineCompletion) Caesura::BackendRegistry::instance().setJobSystem(&inlineJobs);
    run(lua.get(), R"lua(
        calls, onMain = 0, false
        weak_submitter = setmetatable({}, {__mode = 'v'})
        submitter = coroutine.create(function()
            assert(AI.query_async('no endpoint', function()
                calls = calls + 1
                local thread
                thread, onMain = coroutine.running()
            end))
        end)
        assert(coroutine.resume(submitter))
        assert(coroutine.status(submitter) == 'dead')
        weak_submitter[1], submitter = submitter, nil
        collectgarbage('collect')
        assert(weak_submitter[1] == nil, 'completed submitter must be collectible')
    )lua");
    if (!inlineCompletion) {
        jobs.work(0);
        jobs.complete(0);
    }
    run(lua.get(), "assert(calls == 1 and onMain == true, 'callback must execute on main Lua thread')");
    const int sentinel = retainSentinel(lua.get());
    run(lua.get(), "assert(AI.cancel())");
    checkSentinel(lua.get(), sentinel);
    luaL_unref(lua.get(), LUA_REGISTRYINDEX, sentinel);
}

TEST_CASE("AI rejected job submission releases the callback and reports failure") {
    ControlledAiJobs jobs;
    auto lua = makeAiLua();
    jobs.shutdown();
    run(lua.get(), R"lua(
        weak = setmetatable({}, {__mode = 'v'})
        do
            local callback = function() error('rejected callback invoked') end
            weak[1] = callback
            accepted = AI.query_async('rejected', callback)
        end
        collectgarbage('collect')
        assert(accepted == false, 'closed job system must reject admission')
        assert(weak[1] == nil, 'rejected callback must not stay rooted')
    )lua");
    CHECK(jobs.pending.empty());
}
