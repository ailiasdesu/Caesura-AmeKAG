// test_rpc.cpp - RPC module tests (RpcServer, EditorServer)
#include "doctest.h"
#include "rpc/RpcServer.h"
#include "rpc/EditorServer.h"
#include "rpc/api/IRpcServer.h"
#include "rpc/api/IEditorServer.h"
#include "rpc/api/IRpcDispatcher.h"
#include "entry/Engine.h"
#include "script/vm/LuaManager.h"
#include <httplib.h>
#include <nlohmann_json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

extern "C" {
#include <lua.h>
}

using namespace Caesura;

namespace {

class RecordingRpcDispatcher final : public IRpcDispatcher {
public:
    using Handler = std::function<RpcReply(const RpcRequest&)>;

    explicit RecordingRpcDispatcher(Handler handler = {})
        : m_handler(std::move(handler)) {}

    RpcReply dispatch(const RpcRequest& request) override {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_requests.push_back(request);
        }
        if (m_handler) return m_handler(request);
        return {RpcReplyStatus::Ok, {}, {}, {}};
    }

    std::size_t requestCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_requests.size();
    }

    RpcRequest requestAt(std::size_t index) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_requests.at(index);
    }

private:
    Handler m_handler;
    mutable std::mutex m_mutex;
    std::vector<RpcRequest> m_requests;
};

RpcReply successReply(const RpcRequest& request) {
    if (std::holds_alternative<RpcStatusRequest>(request.payload)) {
        return {RpcReplyStatus::Ok, {}, {}, RpcStatusResult{true}};
    }
    if (std::holds_alternative<RpcEvaluateRequest>(request.payload)) {
        return {RpcReplyStatus::Ok, {}, {}, RpcEvaluateResult{"42"}};
    }
    if (std::holds_alternative<RpcGetStateRequest>(request.payload)) {
        return {RpcReplyStatus::Ok, {}, {}, RpcStateResult{"chapter-1"}};
    }
    if (std::holds_alternative<RpcCaptureFrameRequest>(request.payload)) {
        return {RpcReplyStatus::Ok, {}, {}, RpcFrameResult{"ZmFrZS1wbmc="}};
    }
    if (const auto* load = std::get_if<RpcLoadAnimationRequest>(&request.payload)) {
        return {RpcReplyStatus::Ok, {}, {}, RpcAnimationResult{17, load->modelPath}};
    }
    if (std::holds_alternative<RpcInspectLocalRequest>(request.payload) ||
        std::holds_alternative<RpcInspectGlobalRequest>(request.payload)) {
        return {RpcReplyStatus::Ok, {}, {}, RpcInspectionResult{"value"}};
    }
    if (std::holds_alternative<RpcGetDebugStateRequest>(request.payload)) {
        return {RpcReplyStatus::Ok, {}, {}, RpcDebugStateResult{
            RpcDebugRunState::Paused, "scripts/chapter.lua", 27, 91, 2}};
    }
    return {RpcReplyStatus::Ok, {}, {}, {}};
}

class ControlledLineSource {
public:
    RpcLineReadResult readLine(std::string& line) {
        std::unique_lock<std::mutex> lock(m_mutex);
        ++m_readCalls;
        m_reading = true;
        m_changed.notify_all();
        m_changed.wait(lock, [this]() {
            return m_cancelled || m_endOfInput || !m_lines.empty();
        });

        if (m_cancelled) return RpcLineReadResult::Cancelled;
        if (!m_lines.empty()) {
            line = std::move(m_lines.front());
            m_lines.pop_front();
            return RpcLineReadResult::Line;
        }
        return RpcLineReadResult::EndOfInput;
    }

    void cancel() {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_cancelCalls;
        m_cancelled = true;
        m_changed.notify_all();
    }

    void enqueue(std::string line) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lines.push_back(std::move(line));
        m_changed.notify_all();
    }

    void finish() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_endOfInput = true;
        m_changed.notify_all();
    }

    bool waitUntilReading(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_changed.wait_for(lock, timeout, [this]() { return m_reading; });
    }

    int readCalls() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_readCalls;
    }

    int cancelCalls() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_cancelCalls;
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_changed;
    std::deque<std::string> m_lines;
    bool m_cancelled = false;
    bool m_endOfInput = false;
    bool m_reading = false;
    int m_readCalls = 0;
    int m_cancelCalls = 0;
};

RpcLineSource lineSourceFor(const std::shared_ptr<ControlledLineSource>& source) {
    return {
        [source](std::string& line) { return source->readLine(line); },
        [source]() { source->cancel(); },
    };
}

class ScopedModelFiles {
public:
    bool add(const std::string& name) {
        std::error_code error;
        if (!std::filesystem::exists(m_root, error)) {
            if (!std::filesystem::create_directories(m_root, error) || error) {
                return false;
            }
            m_createdRoot = true;
        }

        const std::filesystem::path path = m_root / name;
        if (std::filesystem::exists(path, error) || error) return false;

        std::ofstream output(path, std::ios::binary);
        output.put('x');
        output.close();
        if (!output) {
            std::filesystem::remove(path, error);
            return false;
        }

        m_files.push_back(path);
        return true;
    }

    ~ScopedModelFiles() {
        std::error_code error;
        for (const auto& path : m_files) {
            std::filesystem::remove(path, error);
            error.clear();
        }
        if (m_createdRoot) std::filesystem::remove(m_root, error);
    }

private:
    std::filesystem::path m_root{"models"};
    std::vector<std::filesystem::path> m_files;
    bool m_createdRoot = false;
};

} // namespace

static_assert(std::is_abstract_v<IRpcDispatcher>);
static_assert(std::is_polymorphic_v<IRpcDispatcher>);
static_assert(std::has_virtual_destructor_v<IRpcDispatcher>);

TEST_CASE("RpcLoadAnimationRequest defaults to a visible origin transform") {
    const RpcLoadAnimationRequest request{"models/hero.png"};

    CHECK(request.modelPath == "models/hero.png");
    CHECK(request.x == doctest::Approx(0.0f));
    CHECK(request.y == doctest::Approx(0.0f));
    CHECK(request.scale == doctest::Approx(1.0f));
    CHECK(request.show);
}

TEST_CASE("RpcServer::construct and stop without run") {
    RpcServer rpc;
    rpc.stop();
}

TEST_CASE("RpcServer::isRunning defaults false") {
    RpcServer rpc;
    CHECK(rpc.isRunning() == false);
}

TEST_CASE("IRpcServer interface completeness") {
    RpcServer rpc;
    IRpcServer* iface = &rpc;
    CHECK(iface != nullptr);
    CHECK(iface->isRunning() == false);
}

TEST_CASE("RpcServer::stop interrupts a blocked line read") {
    auto source = std::make_shared<ControlledLineSource>();
    RpcServer rpc(lineSourceFor(source));
    std::thread transport([&rpc]() { rpc.run(); });

    REQUIRE(source->waitUntilReading(std::chrono::milliseconds(500)));
    const auto started = std::chrono::steady_clock::now();
    rpc.stop();
    transport.join();
    const auto elapsed = std::chrono::steady_clock::now() - started;

    CHECK(elapsed < std::chrono::milliseconds(500));
    CHECK_FALSE(rpc.isRunning());
    CHECK(source->cancelCalls() >= 1);
}

TEST_CASE("RpcServer::stop before run is not reset or lost") {
    auto source = std::make_shared<ControlledLineSource>();
    source->finish();
    RpcServer rpc(lineSourceFor(source));

    rpc.stop();
    rpc.run();

    CHECK_FALSE(rpc.isRunning());
    CHECK(source->readCalls() == 0);
    CHECK(source->cancelCalls() >= 1);
}

TEST_CASE("RpcServer::EOF ends transport after queued requests") {
    auto source = std::make_shared<ControlledLineSource>();
    source->enqueue(R"({"id":21,"method":"getState"})");
    source->finish();
    auto dispatcher = std::make_shared<RecordingRpcDispatcher>(successReply);
    RpcServer rpc(lineSourceFor(source));
    rpc.setDispatcher(dispatcher);

    rpc.run();

    CHECK_FALSE(rpc.isRunning());
    REQUIRE(dispatcher->requestCount() == 1);
    CHECK(std::holds_alternative<RpcGetStateRequest>(
        dispatcher->requestAt(0).payload));
}

TEST_CASE("RpcServer::RPC stop replies and does not read another request") {
    auto source = std::make_shared<ControlledLineSource>();
    source->enqueue(R"({"id":22,"method":"stop"})");
    source->enqueue(R"({"id":23,"method":"getState"})");
    source->finish();
    auto dispatcher = std::make_shared<RecordingRpcDispatcher>(successReply);
    RpcServer rpc(lineSourceFor(source));
    rpc.setDispatcher(dispatcher);

    rpc.run();

    CHECK_FALSE(rpc.isRunning());
    CHECK(source->readCalls() == 1);
    REQUIRE(dispatcher->requestCount() == 1);
    CHECK(std::holds_alternative<RpcStopRequest>(
        dispatcher->requestAt(0).payload));
}

TEST_CASE("Engine headless loop honors Lua quit before event early return") {
    EngineConfig config;
    config.headless = true;
    Engine engine(std::move(config));
    REQUIRE(engine.init());

    lua_State* L = engine.lua().state();
    REQUIRE(L != nullptr);
    lua_pushboolean(L, 1);
    lua_setglobal(L, "_CAESURA_QUIT");

    int ownerTicks = 0;
    engine.run([&]() {
        ++ownerTicks;
        if (ownerTicks > 2) engine.quit();
    });

    CHECK(ownerTicks == 1);
    engine.shutdown();
}

// =============================================================================
// Expanded: RpcServer handlers + EditorServer accessors
// =============================================================================

TEST_CASE("RpcServer::pushLog does not crash") {
    RpcServer rpc;
    rpc.pushLog("info", "test");
    rpc.pushLog("error", "oops");
}

TEST_CASE("RpcServer reports structured unavailable without dispatcher") {
    RpcServer rpc;
    const std::string response = rpc.processRequestLine(
        R"({"id":7,"method":"run","script":"return 1"})");
    CHECK(response.find("\"id\":7") != std::string::npos);
    CHECK(response.find("\"status\":\"unavailable\"") != std::string::npos);
    CHECK(response.find("\"code\":\"dispatcher_unavailable\"") != std::string::npos);
}

TEST_CASE("RpcServer accepts a UTF-8 BOM on the first JSON line") {
    RpcServer rpc;
    const std::string request =
        std::string("\xEF\xBB\xBF") + R"({"id":8,"method":"ping"})";
    const std::string response = rpc.processRequestLine(request);
    CHECK(response.find("\"id\":8") != std::string::npos);
    CHECK(response.find("\"result\":\"ok\"") != std::string::npos);
}

TEST_CASE("EditorServer::port returns set port") {
    EditorServer es;
    // port defaults to 0 until start is called
    CHECK(es.port() == 0);
}

TEST_CASE("EditorServer::construct and stop without start") {
    EditorServer es;
    es.stop();
}

TEST_CASE("EditorServer::start and stop joins its worker") {
    EditorServer es;
    REQUIRE(es.start(0));
    CHECK(es.isRunning());
    CHECK(es.port() > 0);
    es.stop();
    CHECK_FALSE(es.isRunning());
}

TEST_CASE("EditorServer::isRunning defaults false") {
    EditorServer es;
    CHECK(es.isRunning() == false);
}

TEST_CASE("EditorServer::setWebRoot no-crash") {
    EditorServer es;
    es.setWebRoot("web-editor/dist");
    es.setArchiveWriterFactory({});
    es.setDispatcher({});
}

TEST_CASE("EditorServer::pushLog no-crash") {
    EditorServer es;
    es.pushLog("info", "test log message");
}

TEST_CASE("IEditorServer interface completeness") {
    EditorServer es;
    IEditorServer* iface = &es;
    CHECK(iface != nullptr);
    CHECK(iface->isRunning() == false);
}

TEST_CASE("RpcServer submits runtime DTOs to dispatcher") {
    auto dispatcher = std::make_shared<RecordingRpcDispatcher>(successReply);
    RpcServer rpc;
    rpc.setDispatcher(dispatcher);

    CHECK(rpc.processRequestLine(
        R"({"id":1,"method":"run","script":"return 9"})")
        .find("\"status\":\"ok\"") != std::string::npos);
    CHECK(rpc.processRequestLine(
        R"({"id":2,"method":"eval","code":"return 6*7"})")
        .find("\"result\":\"42\"") != std::string::npos);
    CHECK(rpc.processRequestLine(
        R"({"id":3,"method":"getFrame","w":640,"h":360})")
        .find("\"frame\":\"ZmFrZS1wbmc=\"") != std::string::npos);
    CHECK(rpc.processRequestLine(
        R"({"id":4,"method":"getState"})")
        .find("\"scene\":\"chapter-1\"") != std::string::npos);
    CHECK(rpc.processRequestLine(
        R"({"id":5,"method":"reload"})")
        .find("\"status\":\"ok\"") != std::string::npos);
    CHECK(rpc.processRequestLine(
        R"({"id":6,"method":"stop"})")
        .find("\"result\":\"ok\"") != std::string::npos);

    REQUIRE(dispatcher->requestCount() == 6);
    const auto run = dispatcher->requestAt(0);
    REQUIRE(std::holds_alternative<RpcRunScriptRequest>(run.payload));
    CHECK(std::get<RpcRunScriptRequest>(run.payload).script == "return 9");

    const auto frame = dispatcher->requestAt(2);
    REQUIRE(std::holds_alternative<RpcCaptureFrameRequest>(frame.payload));
    CHECK(std::get<RpcCaptureFrameRequest>(frame.payload).width == 640);
    CHECK(std::get<RpcCaptureFrameRequest>(frame.payload).height == 360);
    CHECK(std::holds_alternative<RpcReloadScriptsRequest>(
        dispatcher->requestAt(4).payload));
    CHECK(std::holds_alternative<RpcStopRequest>(
        dispatcher->requestAt(5).payload));
}

TEST_CASE("RpcServer maps debugger JSON methods to self-contained DTOs") {
    auto dispatcher = std::make_shared<RecordingRpcDispatcher>(successReply);
    RpcServer rpc;
    rpc.setDispatcher(dispatcher);

    rpc.processRequestLine(
        R"({"id":1,"method":"setBreakpoint","source":"scripts/a.lua","line":12})");
    rpc.processRequestLine(
        R"({"id":2,"method":"removeBreakpoint","source":"scripts/a.lua","line":12})");
    rpc.processRequestLine(R"({"id":3,"method":"clearBreakpoints"})");
    rpc.processRequestLine(R"({"id":4,"method":"continue","pauseId":91})");
    rpc.processRequestLine(R"({"id":5,"method":"stepInto","pauseId":91})");
    rpc.processRequestLine(R"({"id":6,"method":"stepOver","pauseId":91})");
    rpc.processRequestLine(R"({"id":7,"method":"stepOut","pauseId":91})");
    CHECK(rpc.processRequestLine(
        R"({"id":8,"method":"inspectLocal","frame":2,"name":"speaker"})")
        .find("\"value\":\"value\"") != std::string::npos);
    rpc.processRequestLine(
        R"({"id":9,"method":"inspectGlobal","name":"chapter"})");
    const std::string state = rpc.processRequestLine(
        R"({"id":10,"method":"getDebugState"})");

    REQUIRE(dispatcher->requestCount() == 10);
    const auto set = dispatcher->requestAt(0);
    REQUIRE(std::holds_alternative<RpcSetBreakpointRequest>(set.payload));
    CHECK(std::get<RpcSetBreakpointRequest>(set.payload).source == "scripts/a.lua");
    CHECK(std::get<RpcSetBreakpointRequest>(set.payload).line == 12);
    CHECK(std::holds_alternative<RpcRemoveBreakpointRequest>(
        dispatcher->requestAt(1).payload));
    CHECK(std::holds_alternative<RpcClearBreakpointsRequest>(
        dispatcher->requestAt(2).payload));

    const RpcDebugResumeMode modes[] = {
        RpcDebugResumeMode::Continue,
        RpcDebugResumeMode::StepInto,
        RpcDebugResumeMode::StepOver,
        RpcDebugResumeMode::StepOut,
    };
    for (std::size_t i = 0; i < 4; ++i) {
        const auto request = dispatcher->requestAt(i + 3);
        REQUIRE(std::holds_alternative<RpcDebugResumeRequest>(request.payload));
        const auto& resume = std::get<RpcDebugResumeRequest>(request.payload);
        CHECK(resume.pauseId == 91);
        CHECK(resume.mode == modes[i]);
    }

    const auto local = dispatcher->requestAt(7);
    REQUIRE(std::holds_alternative<RpcInspectLocalRequest>(local.payload));
    CHECK(std::get<RpcInspectLocalRequest>(local.payload).frame == 2);
    CHECK(std::get<RpcInspectLocalRequest>(local.payload).name == "speaker");
    CHECK(std::holds_alternative<RpcInspectGlobalRequest>(
        dispatcher->requestAt(8).payload));
    CHECK(std::holds_alternative<RpcGetDebugStateRequest>(
        dispatcher->requestAt(9).payload));
    CHECK(state.find("\"state\":\"paused\"") != std::string::npos);
    CHECK(state.find("\"pauseId\":91") != std::string::npos);
}

TEST_CASE("RpcServer preserves stale pause rejection from dispatcher") {
    auto dispatcher = std::make_shared<RecordingRpcDispatcher>(
        [](const RpcRequest&) {
            return RpcReply{RpcReplyStatus::InvalidRequest, "stale_pause_id",
                "Pause generation is stale", {}};
        });
    RpcServer rpc;
    rpc.setDispatcher(dispatcher);

    const std::string response = rpc.processRequestLine(
        R"({"id":44,"method":"continue","pauseId":8})");
    CHECK(response.find("\"status\":\"invalid_request\"") != std::string::npos);
    CHECK(response.find("\"code\":\"stale_pause_id\"") != std::string::npos);
    CHECK(response.find("Pause generation is stale") != std::string::npos);
}

TEST_CASE("RpcServer preserves dispatcher shutdown unavailable reply") {
    auto dispatcher = std::make_shared<RecordingRpcDispatcher>(
        [](const RpcRequest&) {
            return RpcReply{RpcReplyStatus::Unavailable, "engine_closing",
                "Engine command intake is closed", {}};
        });
    RpcServer rpc;
    rpc.setDispatcher(dispatcher);

    const std::string response = rpc.processRequestLine(
        R"({"id":45,"method":"getDebugState"})");
    CHECK(response.find("\"status\":\"unavailable\"") != std::string::npos);
    CHECK(response.find("\"code\":\"engine_closing\"") != std::string::npos);
    CHECK(response.find("Engine command intake is closed") != std::string::npos);
}

TEST_CASE("EditorServer HTTP worker submits DTOs through dispatcher") {
    auto dispatcher = std::make_shared<RecordingRpcDispatcher>(successReply);
    EditorServer es;
    es.setDispatcher(dispatcher);
    REQUIRE(es.start(0));

    httplib::Client client("127.0.0.1", es.port());
    auto run = client.Post("/api/run", "return 3", "text/plain");
    REQUIRE(run);
    CHECK(run->status == 200);
    CHECK(run->body.find("\"status\":\"ok\"") != std::string::npos);

    auto defaultLoad = client.Post("/api/live2d/load",
        R"({"modelPath":"models/haru.model.json"})", "application/json");
    REQUIRE(defaultLoad);
    CHECK(defaultLoad->status == 200);
    CHECK(defaultLoad->body.find("\"modelId\":17") != std::string::npos);

    auto positionedLoad = client.Post("/api/live2d/load",
        R"({"modelPath":"models/hero \"alt\".PNG","x":-12.5,"y":24,"scale":1.25,"show":false})",
        "application/json");
    REQUIRE(positionedLoad);
    CHECK(positionedLoad->status == 200);
    const nlohmann::json positionedResponse =
        nlohmann::json::parse(positionedLoad->body);
    CHECK(positionedResponse.at("name").get<std::string>() ==
        "models/hero \"alt\".PNG");

    auto reload = client.Post("/api/reload", "", "application/json");
    REQUIRE(reload);
    CHECK(reload->status == 200);
    es.stop();

    REQUIRE(dispatcher->requestCount() == 4);
    const auto runRequest = dispatcher->requestAt(0);
    REQUIRE(std::holds_alternative<RpcRunScriptRequest>(runRequest.payload));
    CHECK(std::get<RpcRunScriptRequest>(runRequest.payload).script == "return 3");
    const auto defaultLoadRequest = dispatcher->requestAt(1);
    REQUIRE(std::holds_alternative<RpcLoadAnimationRequest>(
        defaultLoadRequest.payload));
    const auto& defaultAnimation =
        std::get<RpcLoadAnimationRequest>(defaultLoadRequest.payload);
    CHECK(defaultAnimation.modelPath == "models/haru.model.json");
    CHECK(defaultAnimation.x == doctest::Approx(0.0f));
    CHECK(defaultAnimation.y == doctest::Approx(0.0f));
    CHECK(defaultAnimation.scale == doctest::Approx(1.0f));
    CHECK(defaultAnimation.show);

    const auto positionedLoadRequest = dispatcher->requestAt(2);
    REQUIRE(std::holds_alternative<RpcLoadAnimationRequest>(
        positionedLoadRequest.payload));
    const auto& positionedAnimation =
        std::get<RpcLoadAnimationRequest>(positionedLoadRequest.payload);
    CHECK(positionedAnimation.modelPath == "models/hero \"alt\".PNG");
    CHECK(positionedAnimation.x == doctest::Approx(-12.5f));
    CHECK(positionedAnimation.y == doctest::Approx(24.0f));
    CHECK(positionedAnimation.scale == doctest::Approx(1.25f));
    CHECK_FALSE(positionedAnimation.show);
    CHECK(std::holds_alternative<RpcReloadScriptsRequest>(
        dispatcher->requestAt(3).payload));
}

TEST_CASE("EditorServer rejects invalid animation load transforms before dispatch") {
    auto dispatcher = std::make_shared<RecordingRpcDispatcher>(successReply);
    EditorServer es;
    es.setDispatcher(dispatcher);
    REQUIRE(es.start(0));

    httplib::Client client("127.0.0.1", es.port());
    const std::vector<std::string> invalidBodies = {
        "{",
        "[]",
        "{}",
        R"({"modelPath":3})",
        R"({"modelPath":"hero.png","x":"left"})",
        R"({"modelPath":"hero.png","y":null})",
        R"({"modelPath":"hero.png","scale":0})",
        R"({"modelPath":"hero.png","scale":-1})",
        R"({"modelPath":"hero.png","scale":1e39})",
        R"({"modelPath":"hero.png","show":1})",
    };

    for (const auto& body : invalidBodies) {
        CAPTURE(body);
        auto response =
            client.Post("/api/live2d/load", body, "application/json");
        REQUIRE(response);
        CHECK(response->status == 400);
        CHECK(response->body.find("\"status\":\"invalid_request\"") !=
            std::string::npos);
        CHECK(response->body.find("\"code\":\"invalid_animation_request\"") !=
            std::string::npos);
    }

    es.stop();
    CHECK(dispatcher->requestCount() == 0);
}

TEST_CASE("EditorServer lists static animation images case-insensitively as valid JSON") {
    ScopedModelFiles files;
    REQUIRE(files.add("caesura_rpc_static_sprite.PnG"));
    REQUIRE(files.add("caesura_rpc_static_portrait.JpG"));
    REQUIRE(files.add("caesura_rpc_static_photo.JpEg"));
    REQUIRE(files.add("caesura_rpc_static_bitmap.BmP"));
    REQUIRE(files.add("caesura_rpc_static_ignore.TxT"));

    EditorServer es;
    REQUIRE(es.start(0));

    httplib::Client client("127.0.0.1", es.port());
    auto response = client.Get("/api/live2d/models");
    REQUIRE(response);
    CHECK(response->status == 200);
    es.stop();

    const nlohmann::json models = nlohmann::json::parse(response->body);
    REQUIRE(models.is_array());

    std::vector<std::pair<std::string, std::string>> entries;
    std::vector<std::string> paths;
    for (const auto& model : models) {
        REQUIRE(model.is_object());
        REQUIRE(model.contains("name"));
        REQUIRE(model.contains("path"));
        const std::string name = model.at("name").get<std::string>();
        const std::string path = model.at("path").get<std::string>();
        entries.emplace_back(path, name);
        paths.push_back(path);
    }
    CHECK(std::is_sorted(paths.begin(), paths.end()));

    const auto containsModel = [&entries](const std::string& path,
                                          const std::string& name) {
        return std::find(entries.begin(), entries.end(),
            std::pair<std::string, std::string>{path, name}) != entries.end();
    };
    CHECK(containsModel("models/caesura_rpc_static_sprite.PnG",
        "caesura_rpc_static_sprite.PnG"));
    CHECK(containsModel("models/caesura_rpc_static_portrait.JpG",
        "caesura_rpc_static_portrait.JpG"));
    CHECK(containsModel("models/caesura_rpc_static_photo.JpEg",
        "caesura_rpc_static_photo.JpEg"));
    CHECK(containsModel("models/caesura_rpc_static_bitmap.BmP",
        "caesura_rpc_static_bitmap.BmP"));
    CHECK_FALSE(containsModel("models/caesura_rpc_static_ignore.TxT",
        "caesura_rpc_static_ignore.TxT"));
}

TEST_CASE("EditorServer reports dispatcher unavailable over HTTP") {
    EditorServer es;
    REQUIRE(es.start(0));

    httplib::Client client("127.0.0.1", es.port());
    auto response = client.Get("/api/status");
    REQUIRE(response);
    CHECK(response->status == 503);
    CHECK(response->body.find("\"status\":\"unavailable\"") != std::string::npos);
    CHECK(response->body.find("\"code\":\"dispatcher_unavailable\"") !=
        std::string::npos);
    es.stop();
}
