// ===========================================================================
//  Caesura (AmeKAG) -- RpcServer implementation
//  stdin/stdout JSON-RPC — simplest possible protocol.
//  Each line is a complete JSON object, \n delimited.
// ===========================================================================

#include "RpcServer.h"
#include "../Scripting/LuaManager.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <cstdio>
#include <ctime>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
namespace Caesura {

// =========================================================================
// Singleton
// =========================================================================

RpcServer& RpcServer::instance() {
    static RpcServer s;
    return s;
}

// =========================================================================
// Lifecycle
// =========================================================================

void RpcServer::run() {
    m_running = true;
    std::string line;

    fprintf(stderr, "[RpcServer] JSON-RPC ready on stdin/stdout\n");

    while (m_running && std::getline(std::cin, line)) {
        if (line.empty()) continue;
        if (line[0] != '{') continue;  // skip non-JSON

        std::string response = handleRequest(line);
        if (!response.empty()) {
            writeLine(response);
        }

        if (m_shutdownRequested) {
            m_running = false;
            break;
        }
    }

    fprintf(stderr, "[RpcServer] Shutdown.\n");
}

void RpcServer::stop() {
    m_running = false;
    m_shutdownRequested = true;
    // Write a dummy line to unblock getline
    std::cout << std::endl;
}

// =========================================================================
// Log buffer (thread-safe output)
// =========================================================================

void RpcServer::pushLog(const std::string& level, const std::string& message) {
    std::ostringstream json;
    json << "{\"event\":\"log\",\"level\":\"" << jsonEscape(level)
         << "\",\"message\":\"" << jsonEscape(message) << "\"}";
    writeLine(json.str());
}

void RpcServer::writeLine(const std::string& json) {
    std::lock_guard<std::mutex> lock(m_writeMutex);
    std::cout << json << std::endl;
    std::cout.flush();
}

// =========================================================================
// Request dispatcher
// =========================================================================

static int parseId(const std::string& json) {
    // Quick and dirty: find "id":N
    size_t pos = json.find("\"id\":");
    if (pos == std::string::npos) return 0;
    pos += 5;
    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    // Read number
    int val = 0;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
        val = val * 10 + (json[pos] - '0');
        pos++;
    }
    return val;
}

static std::string extractField(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) {
        // Try unquoted value
        search = "\"" + key + "\":";
        pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        // Skip whitespace
        while (pos < json.size() && json[pos] == ' ') pos++;
        // Read until , or } or \n
        size_t end = json.find_first_of(",}", pos);
        if (end == std::string::npos) end = json.size();
        std::string val = json.substr(pos, end - pos);
        // Trim whitespace
        while (!val.empty() && val.front() == ' ') val.erase(0, 1);
        while (!val.empty() && val.back() == ' ') val.pop_back();
        return val;
    }
    pos += search.size();
    size_t end = json.find('\"', pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

static std::string extractMethod(const std::string& json) {
    return extractField(json, "method");
}

std::string RpcServer::handleRequest(const std::string& jsonLine) {
    std::string method = extractMethod(jsonLine);
    int id = parseId(jsonLine);

    if (method == "ping")    return handlePing(id);
    if (method == "run")     return handleRun(id, extractField(jsonLine, "script"));
    if (method == "stop")    return handleStop(id);
    if (method == "logs")    return handleLogs(id);
    if (method == "assets")  return handleAssets(id, extractField(jsonLine, "type"));
    if (method == "eval")    return handleEval(id, extractField(jsonLine, "code"));
    if (method == "getFrame") return handleGetFrame(id,
        std::stoi(extractField(jsonLine, "w").empty() ? "0" : extractField(jsonLine, "w")),
        std::stoi(extractField(jsonLine, "h").empty() ? "0" : extractField(jsonLine, "h")));
    if (method == "getState") return handleGetState(id);

    // Unknown method
    std::ostringstream err;
    err << "{\"id\":" << id << ",\"error\":\"Unknown method: " << jsonEscape(method) << "\"}";
    return err.str();
}

// =========================================================================
// Handlers
// =========================================================================

std::string RpcServer::handlePing(int id) {
    std::ostringstream out;
    out << "{\"id\":" << id << ",\"result\":\"ok\",\"engine\":\"CaesuraAmeKAG\"}";
    return out.str();
}

std::string RpcServer::handleRun(int id, const std::string& script) {
    lua_State* L = m_L ? m_L : LuaManager::instance().state();
    if (!L) {
        std::ostringstream err;
        err << "{\"id\":" << id << ",\"error\":\"Lua not initialized\"}";
        return err.str();
    }
    if (script.empty()) {
        std::ostringstream err;
        err << "{\"id\":" << id << ",\"error\":\"Empty script\"}";
        return err.str();
    }

    // Write to temp file and execute
    std::string tmpPath = "temp_editor_scene.lua";
    {
        std::ofstream ofs(tmpPath);
        ofs << script;
    }

    pushLog("info", "Running scene script...");

    if (luaL_dofile(L, tmpPath.c_str()) != LUA_OK) {
        std::string err = lua_tostring(L, -1);
        lua_pop(L, 1);
        pushLog("error", err);
        fs::remove(tmpPath);
        std::ostringstream out;
        out << "{\"id\":" << id << ",\"status\":\"error\",\"message\":\"" << jsonEscape(err) << "\"}";
        return out.str();
    }

    pushLog("info", "Scene script completed.");
    fs::remove(tmpPath);

    std::ostringstream out;
    out << "{\"id\":" << id << ",\"status\":\"ok\"}";
    return out.str();
}

std::string RpcServer::handleStop(int id) {
    lua_State* L = m_L ? m_L : LuaManager::instance().state();
    if (L) {
        lua_pushboolean(L, 1);
        lua_setglobal(L, "_CAESURA_QUIT");
    }
    pushLog("info", "Stop requested.");
    std::ostringstream out;
    out << "{\"id\":" << id << ",\"result\":\"ok\"}";
    return out.str();
}

std::string RpcServer::handleLogs(int id) {
    // For now, return empty. Real implementation would buffer logs.
    std::ostringstream out;
    out << "{\"id\":" << id << ",\"logs\":[]}";
    return out.str();
}

std::string RpcServer::handleAssets(int id, const std::string& type) {
    std::ostringstream out;
    out << "{\"id\":" << id << ",\"assets\":[";

    auto addDir = [&](const std::string& dir, const std::string& kind) {
        if (!type.empty() && type != kind) return;
        std::string path = "assets/" + dir;
        if (!fs::exists(path)) return;
        try {
            bool first = true;
            for (const auto& entry : fs::directory_iterator(path)) {
                if (!entry.is_regular_file()) continue;
                if (!first) out << ",";
                first = false;
                std::string name = entry.path().filename().string();
                out << "{\"path\":\"assets/" << dir << "/" << name
                    << "\",\"name\":\"" << name
                    << "\",\"type\":\"" << kind << "\"}";
            }
        } catch (...) {}
    };

    addDir("bg", "image");
    addDir("char", "image");
    addDir("ui", "image");
    addDir("bgm", "audio");
    addDir("voice", "audio");
    addDir("se", "audio");
    addDir("scripts", "script");

    out << "]}";
    return out.str();
}

std::string RpcServer::handleEval(int id, const std::string& code) {
    lua_State* L = m_L ? m_L : LuaManager::instance().state();
    if (!L) {
        std::ostringstream err;
        err << "{\"id\":" << id << ",\"error\":\"Lua not initialized\"}";
        return err.str();
    }
    if (code.empty()) {
        std::ostringstream err;
        err << "{\"id\":" << id << ",\"error\":\"Empty code\"}";
        return err.str();
    }

    if (luaL_dostring(L, code.c_str()) != LUA_OK) {
        std::string err = lua_tostring(L, -1);
        lua_pop(L, 1);
        std::ostringstream out;
        out << "{\"id\":" << id << ",\"status\":\"error\",\"message\":\"" << jsonEscape(err) << "\"}";
        return out.str();
    }

    // Get return value if any
    std::string result = "nil";
    if (lua_gettop(L) > 0) {
        if (lua_isstring(L, -1)) result = lua_tostring(L, -1);
        else if (lua_isnumber(L, -1)) result = std::to_string(lua_tonumber(L, -1));
        else if (lua_isboolean(L, -1)) result = lua_toboolean(L, -1) ? "true" : "false";
        lua_pop(L, 1);
    }

    std::ostringstream out;
    out << "{\"id\":" << id << ",\"status\":\"ok\",\"result\":\"" << jsonEscape(result) << "\"}";
    return out.str();
}

std::string RpcServer::handleGetFrame(int id, int w, int h) {
    if (!m_frameCaptureCb) {
        std::ostringstream err;
        err << "{\"id\":" << id << ",\"error\":\"Frame capture not available (editor mode only)\"}";
        return err.str();
    }
    if (w <= 0) w = 1280;
    if (h <= 0) h = 720;
    std::string b64 = m_frameCaptureCb(w, h);
    if (b64.empty()) {
        std::ostringstream err;
        err << "{\"id\":" << id << ",\"error\":\"Screenshot capture failed\"}";
        return err.str();
    }
    std::ostringstream out;
    out << "{\"id\":" << id << ",\"frame\":\"" << b64 << "\"}";
    return out.str();
}

std::string RpcServer::handleGetState(int id) {
    lua_State* L = m_L ? m_L : LuaManager::instance().state();
    std::ostringstream out;
    out << "{\"id\":" << id << ",\"state\":{";

    if (L) {
        // Read current scene name
        lua_getglobal(L, "_KAG_SceneName");
        if (lua_isstring(L, -1)) {
            out << "\"scene\":\"" << jsonEscape(lua_tostring(L, -1)) << "\"";
        }
        lua_pop(L, 1);
    }

    out << "}}";
    return out.str();
}

// =========================================================================
// JSON utility
// =========================================================================

std::string RpcServer::jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

} // namespace Caesura