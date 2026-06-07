#pragma once
// ===========================================================================
//  Caesura (AmeKAG) -- RpcServer
//  stdin/stdout JSON-RPC for IDE/Editor integration.
//  Zero network, zero ports, zero config. Just pipe JSON lines.
// ===========================================================================

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <sstream>

struct lua_State;

namespace Caesura {

class RpcServer {
public:
    static RpcServer& instance();

    RpcServer(const RpcServer&) = delete;
    RpcServer& operator=(const RpcServer&) = delete;

    // Set Lua state (called by Engine)
    void setLuaState(lua_State* L) { m_L = L; }

    // Run the JSON-RPC loop (blocks until stop or stdin EOF)
    void run();

    // Signal stop from outside
    void stop();

    // Push a log entry to the output stream (thread-safe)
    void pushLog(const std::string& level, const std::string& message);

private:
    RpcServer() = default;

    std::string handleRequest(const std::string& jsonLine);
    std::string handlePing(int id);
    std::string handleRun(int id, const std::string& script);
    std::string handleStop(int id);
    std::string handleLogs(int id);
    std::string handleAssets(int id, const std::string& type);
    std::string handleEval(int id, const std::string& code);
    std::string handleGetState(int id);

    // Write a JSON line to stdout (thread-safe via mutex)
    void writeLine(const std::string& json);

    // Escape string for JSON
    static std::string jsonEscape(const std::string& s);

    lua_State*  m_L = nullptr;
    std::atomic<bool> m_running{false};
    std::mutex   m_writeMutex;
    bool         m_shutdownRequested = false;
};

} // namespace Caesura