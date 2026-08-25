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
    // Optional bearer token; when set, requests must present
    // "Authorization: Bearer <token>".
    void setAuthToken(const std::string& token) override;

    // --- Secure-by-default auth gate (Sprint 1 t4) ---------------------
    // /api/eval and /api/run execute ARBITRARY Lua and /api/build,
    // /api/package/* write files, so an unauthenticated HTTP editor is a
    // local-code-execution primitive for every other process on the box.
    // start() therefore requires a bearer token: when none was configured
    // it GENERATES one, prints it to stderr and writes it to
    // <cwd>/.caesura-editor-token (owner-only permissions where the platform
    // supports them) so the editor frontend can pick it up.
    //
    // The escape hatch is explicit and loud: setInsecureNoAuth(true) (wired
    // to --editor-insecure) or CAESURA_EDITOR_INSECURE=1 in the environment
    // disables the gate and warns on every start. Not part of IEditorServer:
    // it is a deployment knob of this implementation, not protocol surface.
    void setInsecureNoAuth(bool insecure);
    bool insecureNoAuth() const;
    // Token actually in force after start() (configured or generated).
    std::string authToken() const;
    // Absolute path of the token file written by start(); empty when none
    // was written (configured token or insecure mode).
    std::string authTokenFile() const;

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
    std::string m_authToken;
    // Escape hatch for the secure-by-default gate (--editor-insecure /
    // CAESURA_EDITOR_INSECURE=1). Atomic: read from the HTTP worker thread.
    std::atomic<bool> m_insecureNoAuth{false};
    std::string m_authTokenFile;  // guarded by m_dispatcherMutex

    // Generate a cryptographically-unpredictable hex token, or an empty
    // string when no entropy source is available (caller fails closed).
    static std::string generateAuthToken();
    // Ensure a token exists before the server accepts requests. Returns
    // false when a token was required but could not be established.
    bool ensureAuthToken();

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
