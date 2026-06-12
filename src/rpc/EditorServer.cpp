// ===========================================================================
//  Caesura (AmeKAG) -- EditorServer implementation (Track 4)
// ===========================================================================

#include "EditorServer.h"
#include "../../external/cpp-httplib/httplib.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "../di/BackendRegistry.h"
#include "../archive/CARCWriter.h"
#include "../script/vm/LuaManager.h"
#include "../script/vm/LuaManager.h"
#include <cstdio>
#include <ctime>
#include <sstream>
#include <fstream>
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
    // GET /api/status -- detailed engine status (R1.1)
    // ---------------------------------------------------------------------
    svr.Get("/api/status", [this](const httplib::Request&, httplib::Response& res) {
        const char* luaOk = (m_L || BackendRegistry::instance().getLuaState()) ? "true" : "false";
        char buf[256];
        snprintf(buf, sizeof(buf),
            "{\"status\":\"ok\",\"engine\":\"CaesuraAmeKAG\",\"lua\":%s,\"port\":%d}",
            luaOk, m_port);
        res.set_content(buf, "application/json");
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

        lua_State* L = m_L ? m_L : BackendRegistry::instance().getLuaState();
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
        lua_State* L = m_L ? m_L : BackendRegistry::instance().getLuaState();
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

    // ---------------------------------------------------------------------
    // GET /api/live2d/models -- list available Live2D models (R1.2)
    // ---------------------------------------------------------------------
    svr.Get("/api/live2d/models", [](const httplib::Request&, httplib::Response& res) {
        std::string json = "[";
        bool first = true;
        const char* dirs[] = {"models", "assets/models", "assets/live2d"};
        for (const char* dir : dirs) {
            if (!fs::exists(dir)) continue;
            try {
                for (const auto& entry : fs::directory_iterator(dir)) {
                    if (!entry.is_regular_file()) continue;
                    std::string name = entry.path().filename().string();
                    // Filter for Live2D model files
                    bool isModel = false;
                    if (name.size() > 4) {
                        std::string ext = name.substr(name.size() - 4);
                        if (ext == ".moc" || ext == ".json" || name.find(".model") != std::string::npos) isModel = true;
                    }
                    if (!isModel) continue;
                    if (!first) json += ",";
                    first = false;
                    std::string path = (std::string(dir) + "/" + name);
                    json += "{\"name\":\"" + name + "\",\"path\":\"" + path + "\"}";
                }
            } catch (...) {}
        }
        json += "]";
        res.set_content(json, "application/json");
    });

    // ---------------------------------------------------------------------
    // POST /api/live2d/load -- load a Live2D model (R1.2)
    // ---------------------------------------------------------------------
    svr.Post("/api/live2d/load", [](const httplib::Request& req, httplib::Response& res) {
        // Parse modelPath from JSON body
        std::string body = req.body;
        std::string modelPath;
        auto pos = body.find("\"modelPath\"");
        if (pos != std::string::npos) {
            auto start = body.find("\"", pos + 12);
            if (start != std::string::npos) {
                auto end = body.find("\"", start + 1);
                if (end != std::string::npos) {
                    modelPath = body.substr(start + 1, end - start - 1);
                }
            }
        }

        if (modelPath.empty()) {
            res.set_content("{\"error\":\"modelPath required\"}", "application/json");
            res.status = 400;
            return;
        }

        auto* anim = BackendRegistry::instance().getAnimationBackend();
        if (!anim) {
            res.set_content("{\"error\":\"Animation backend not available\"}", "application/json");
            res.status = 500;
            return;
        }

        anim->init();
        int handle = anim->loadModel(modelPath.c_str(), modelPath.c_str());
        if (handle > 0) {
            char buf[128];
            snprintf(buf, sizeof(buf),
                "{\"status\":\"ok\",\"modelId\":%d,\"name\":\"%s\"}",
                handle, modelPath.c_str());
            res.set_content(buf, "application/json");
        } else {
            res.set_content("{\"error\":\"Failed to load model\"}", "application/json");
            res.status = 500;
        }
    });

    // ---------------------------------------------------------------------
    // POST /api/build -- one-click CARC packaging (R1.3)
    // ---------------------------------------------------------------------
    svr.Post("/api/build", [](const httplib::Request& req, httplib::Response& res) {
        std::string outputPath = "build/game.carc";
        std::string keyPath = "build/game.key";

        // Parse optional outputPath and keyPath from body
        std::string body = req.body;
        auto findParam = [&](const std::string& name) -> std::string {
            auto pos = body.find("\"" + name + "\"");
            if (pos == std::string::npos) return "";
            auto start = body.find("\"", pos + name.size() + 3);
            if (start == std::string::npos) return "";
            auto end = body.find("\"", start + 1);
            if (end == std::string::npos) return "";
            return body.substr(start + 1, end - start - 1);
        };
        std::string customOutput = findParam("outputPath");
        std::string customKey = findParam("keyPath");
        if (!customOutput.empty()) outputPath = customOutput;
        if (!customKey.empty()) keyPath = customKey;

        // Collect files from scripts/ and assets/
        std::vector<std::pair<std::string, std::string>> files; // relPath, diskPath
        for (const char* dir : {"scripts", "assets"}) {
            if (!fs::exists(dir)) continue;
            try {
                for (const auto& entry : fs::recursive_directory_iterator(dir)) {
                    if (!entry.is_regular_file()) continue;
                    std::string rel = entry.path().string();
                    // Normalize to forward slashes
                    for (auto& c : rel) if (c == '\\') c = '/';
                    files.push_back({rel, entry.path().string()});
                }
            } catch (...) {}
        }

        if (files.empty()) {
            res.set_content("{\"error\":\"No files to package\"}", "application/json");
            res.status = 400;
            return;
        }

        // Create output directory
        fs::create_directories("build");

        // Package using CARCWriter
        Caesura::carc::CARCWriter writer;
        if (!writer.create(outputPath, keyPath, keyPath + ".pub")) {
            res.set_content("{\"error\":\"Failed to create CARC archive\"}", "application/json");
            res.status = 500;
            return;
        }

        for (const auto& [relPath, diskPath] : files) {
            std::ifstream ifs(diskPath, std::ios::binary);
            if (!ifs.is_open()) continue;
            std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)),
                                       std::istreambuf_iterator<char>());
            writer.addFile(relPath, data.data(), data.size());
        }

        if (!writer.finalize()) {
            res.set_content("{\"error\":\"Failed to finalize CARC archive\"}", "application/json");
            res.status = 500;
            return;
        }

        auto fileSize = fs::file_size(outputPath);
        char buf[256];
        snprintf(buf, sizeof(buf),
            "{\"status\":\"ok\",\"path\":\"%s\",\"size\":%llu,\"files\":%zu}",
            outputPath.c_str(), (unsigned long long)fileSize, files.size());
        res.set_content(buf, "application/json");
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
