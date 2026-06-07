// ===========================================================================
//  Caesura (AmeKAG) -- EditorServer implementation (Track 4)
// ===========================================================================

#include "EditorServer.h"
#include "../../external/cpp-httplib/httplib.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "../Core/BackendRegistry.h"
#include "../Core/Engine.h"
#include "../Scripting/LuaManager.h"
#include <cstdio>
#include <ctime>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;
namespace Caesura {

// =========================================================================
// Singleton
// =========================================================================

EditorServer& EditorServer::instance() {
    static EditorServer s;
    return s;
}

// =========================================================================
// Lifecycle
// =========================================================================

bool EditorServer::start(int port) {
    if (m_running) {
        printf("[EditorServer] Already running on port %d\n", m_port);
        return true;
    }

    m_port = port;
    m_running = true;
    m_thread = std::thread(&EditorServer::serverLoop, this, port);

    // Give the server a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    if (m_running) {
        printf("[EditorServer] Started on http://localhost:%d\n", m_port);
        return true;
    }
    return false;
}

void EditorServer::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
    printf("[EditorServer] Stopped.\n");
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
    httplib::Server svr;

    // ---------------------------------------------------------------------
    // CORS middleware - allow web editor from any origin
    // ---------------------------------------------------------------------
    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
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
    // GET /api/assets -- list project assets
    // ---------------------------------------------------------------------
    svr.Get("/api/assets", [](const httplib::Request& req, httplib::Response& res) {
        std::string type = req.get_param_value("type");  // "image", "audio", "script", "" = all

        auto pushType = [&](const std::string& dir, const std::string& kind) {
            if (!type.empty() && type != kind) return;
            std::string path = "assets/" + dir;
            if (!fs::exists(path)) return;
            try {
                for (const auto& entry : fs::directory_iterator(path)) {
                    if (entry.is_regular_file()) {
                        std::string name = entry.path().filename().string();
                        res.set_chunked_content_provider("application/json",
                            [](size_t, httplib::DataSink&) { return true; });
                    }
                }
            } catch (...) {}
        };

        // Build JSON array
        std::string json = "[";
        bool first = true;

        auto addFiles = [&](const std::string& dir, const std::string& kind) {
            if (!type.empty() && type != kind) return;
            std::string path = "assets/" + dir;
            if (!fs::exists(path)) return;
            try {
                for (const auto& entry : fs::directory_iterator(path)) {
                    if (entry.is_regular_file()) {
                        if (!first) json += ",";
                        first = false;
                        std::string name = entry.path().filename().string();
                        std::string relPath = "assets/" + dir + "/" + name;
                        // Escape for JSON
                        std::string escapedPath = relPath;
                        std::string escapedName = name;
                        json += "{\"path\":\"" + escapedPath + "\",\"name\":\"" + escapedName + "\",\"type\":\"" + kind + "\"}";
                    }
                }
            } catch (...) {}
        };

        addFiles("bg", "image");
        addFiles("char", "image");
        addFiles("ui", "image");
        addFiles("bgm", "audio");
        addFiles("voice", "audio");
        addFiles("se", "audio");
        addFiles("scripts", "script");

        json += "]";
        res.set_content(json, "application/json");
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

        lua_State* L = m_L ? m_L : LuaManager::instance().state();
        if (!L) {
            res.set_content("{\"error\":\"Lua not initialized\"}", "application/json");
            res.status = 500;
            return;
        }

        // Write script to temp file and execute
        std::string tmpPath = "temp_editor_scene.lua";
        {
            std::ofstream ofs(tmpPath);
            ofs << script;
        }

        pushLog("info", "Running scene script...");

        // Capture Lua output via a custom print
        lua_getglobal(L, "print");
        // Temporarily override print to capture output
        // For now, just run the script
        if (luaL_dofile(L, tmpPath.c_str()) != LUA_OK) {
            std::string err = lua_tostring(L, -1);
            lua_pop(L, 1);
            pushLog("error", err);
            res.set_content("{\"status\":\"error\",\"message\":\"" + err + "\"}", "application/json");
        } else {
            pushLog("info", "Scene script completed.");
            res.set_content("{\"status\":\"ok\"}", "application/json");
        }

        // Clean up temp file
        fs::remove(tmpPath);
    });

    // ---------------------------------------------------------------------
    // POST /api/stop -- stop execution
    // ---------------------------------------------------------------------
    svr.Post("/api/stop", [this](const httplib::Request&, httplib::Response& res) {
        lua_State* L = m_L ? m_L : LuaManager::instance().state();
        if (L) {
            lua_pushboolean(L, 1);
            lua_setglobal(L, "_CAESURA_QUIT");
        }
        pushLog("info", "Stop requested.");
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    // ---------------------------------------------------------------------
    // GET /api/logs -- recent log entries
    // ---------------------------------------------------------------------
    svr.Get("/api/logs", [this](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(m_logMutex);

        std::string json = "[";
        for (size_t i = 0; i < m_logs.size(); i++) {
            if (i > 0) json += ",";
            auto& entry = m_logs[i];
            json += "{\"level\":\"" + entry.level + "\",\"message\":\"" + entry.message + "\",\"time\":\"" + entry.time + "\"}";
        }
        json += "]";
        res.set_content(json, "application/json");
    });

    // ---------------------------------------------------------------------
    // Static file serving -- web editor frontend
    // ---------------------------------------------------------------------
    if (!m_webRoot.empty() && fs::exists(m_webRoot)) {
        svr.set_mount_point("/", m_webRoot);
        printf("[EditorServer] Serving web editor from: %s\n", m_webRoot.c_str());
    }

    // ---------------------------------------------------------------------
    // Start listening
    // ---------------------------------------------------------------------
    printf("[EditorServer] Binding to port %d...\n", port);
    if (!svr.listen("127.0.0.1", port)) {
        fprintf(stderr, "[EditorServer] Failed to bind to port %d\n", port);
        m_running = false;
    }
}

} // namespace Caesura
