#pragma once
#include "api/IEditorServer.h"
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
#include <memory>
#include <utility>

namespace httplib { class Server; }

namespace Caesura {

class EditorServer : public IEditorServer {
public:
    EditorServer();
    ~EditorServer() override;

    EditorServer(const EditorServer&) = delete;
    EditorServer& operator=(const EditorServer&) = delete;

    // Start HTTP server on given port. Returns false if port is in use.
    bool start(int port = 9876) override;

    // Stop server and join thread.
    void stop() override;

    bool isRunning() const override { return m_running.load(); }
    int  port() const override { return m_port; }

    // Post a log entry for the web editor to poll.
    void pushLog(const std::string& level, const std::string& message) override;

    void setDispatcher(std::shared_ptr<IRpcDispatcher> dispatcher) override;

    // Set the web editor static files directory.
    void setWebRoot(const std::string& path) override { m_webRoot = path; }

    void setArchiveWriterFactory(ArchiveWriterFactory factory) override {
        m_archiveWriterFactory = std::move(factory);
    }
private:
    void serverLoop(int port);
    RpcReply dispatchRequest(RpcRequest request) const;

    std::thread      m_thread;
    std::unique_ptr<httplib::Server> m_server;
    std::atomic<bool> m_running{false};
    int              m_port = 0;
    std::string      m_webRoot;
    ArchiveWriterFactory m_archiveWriterFactory;
    mutable std::mutex m_dispatcherMutex;
    std::shared_ptr<IRpcDispatcher> m_dispatcher;

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
