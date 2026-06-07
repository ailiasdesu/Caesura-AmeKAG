#pragma once
// ===========================================================================
//  Caesura (AmeKAG) -- EditorServer (Track 4)
//  Embedded HTTP server for the Web-based visual editor.
//  Uses cpp-httplib (header-only). Runs on a background thread.
// ===========================================================================

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>

struct lua_State;

namespace Caesura {

class EditorServer {
public:
    static EditorServer& instance();

    EditorServer(const EditorServer&) = delete;
    EditorServer& operator=(const EditorServer&) = delete;

    // Start HTTP server on given port. Returns false if port is in use.
    bool start(int port = 9876);

    // Stop server and join thread.
    void stop();

    bool isRunning() const { return m_running.load(); }
    int  port() const { return m_port; }

    // Post a log entry for the web editor to poll.
    void pushLog(const std::string& level, const std::string& message);

    // Set the Lua state for scene execution (called by Engine::init).
    void setLuaState(lua_State* L) { m_L = L; }

    // Set the web editor static files directory.
    void setWebRoot(const std::string& path) { m_webRoot = path; }

private:
    EditorServer() = default;
    void serverLoop(int port);

    std::thread      m_thread;
    std::atomic<bool> m_running{false};
    int              m_port = 0;
    lua_State*       m_L = nullptr;
    std::string      m_webRoot;

    // Log buffer (ring buffer, max 200 entries)
    struct LogEntry {
        std::string level;
        std::string message;
        std::string time;
    };
    std::mutex       m_logMutex;
    std::vector<LogEntry> m_logs;
    static constexpr int MAX_LOGS = 200;
};

} // namespace Caesura
