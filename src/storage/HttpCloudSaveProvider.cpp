// HttpCloudSaveProvider.cpp — see header. Uses httplib (header-only,
// exposed to every module via CaesarBuildOptions).
#include "HttpCloudSaveProvider.h"
#include "LocalFileSaveProvider.h"
#include <httplib.h>
#include <cstdio>

namespace Caesura {

HttpCloudSaveProvider::HttpCloudSaveProvider(std::string endpoint, int timeoutMs)
    : m_endpoint(std::move(endpoint))
    , m_timeoutMs(timeoutMs > 0 ? timeoutMs : 8000)
    , m_local(std::make_unique<LocalFileSaveProvider>()) {}

std::string HttpCloudSaveProvider::safeName(const std::string& slotPath) {
    // Strip any directory component: "saves/slot_3.json" -> "slot_3.json".
    const auto pos = slotPath.find_last_of("/\\");
    return pos == std::string::npos ? slotPath : slotPath.substr(pos + 1);
}

std::string HttpCloudSaveProvider::readFile(const std::string& path) {
    return m_local->readFile(path);
}

bool HttpCloudSaveProvider::writeFile(const std::string& path,
                                      const std::string& content) {
    return m_local->writeFile(path, content);
}

bool HttpCloudSaveProvider::deleteFile(const std::string& path) {
    return m_local->deleteFile(path);
}

std::vector<std::string> HttpCloudSaveProvider::listFiles(const std::string& pattern) {
    return m_local->listFiles(pattern);
}

// -- Cloud sync -------------------------------------------------------------

namespace {
// Split "http://host:port/prefix" into (host, port, prefix). Returns false
// on malformed input.
bool splitEndpoint(const std::string& endpoint, std::string& host,
                   int& port, std::string& prefix) {
    host = endpoint;
    const std::string scheme = "http://";
    if (host.rfind(scheme, 0) != 0) return false;
    host = host.substr(scheme.size());
    auto slash = host.find('/');
    if (slash != std::string::npos) {
        prefix = host.substr(slash);
        host = host.substr(0, slash);
    }
    port = 80;
    auto colon = host.rfind(':');
    if (colon != std::string::npos) {
        port = std::atoi(host.substr(colon + 1).c_str());
        host = host.substr(0, colon);
    }
    return !host.empty() && port > 0;
}
} // namespace

bool HttpCloudSaveProvider::httpPut(const std::string& name,
                                    const std::string& body) {
    if (m_endpoint.empty()) return false;
    std::string host, prefix;
    int port = 0;
    if (!splitEndpoint(m_endpoint, host, port, prefix)) return false;

    httplib::Client cli(host, port);
    cli.set_connection_timeout(2, 0);
    cli.set_read_timeout(m_timeoutMs / 1000, (m_timeoutMs % 1000) * 1000);
    cli.set_write_timeout(5, 0);

    auto res = cli.Put(prefix + "/" + name, body, "application/octet-stream");
    return res && res->status == 200;
}

std::string HttpCloudSaveProvider::httpGet(const std::string& name) {
    if (m_endpoint.empty()) return std::string();
    std::string host, prefix;
    int port = 0;
    if (!splitEndpoint(m_endpoint, host, port, prefix)) return std::string();

    httplib::Client cli(host, port);
    cli.set_connection_timeout(2, 0);
    cli.set_read_timeout(m_timeoutMs / 1000, (m_timeoutMs % 1000) * 1000);

    auto res = cli.Get(prefix + "/" + name);
    if (!res || res->status != 200) return std::string();
    return res->body;
}

bool HttpCloudSaveProvider::httpDelete(const std::string& name) {
    if (m_endpoint.empty()) return false;
    std::string host, prefix;
    int port = 0;
    if (!splitEndpoint(m_endpoint, host, port, prefix)) return false;

    httplib::Client cli(host, port);
    cli.set_connection_timeout(2, 0);
    cli.set_read_timeout(m_timeoutMs / 1000, (m_timeoutMs % 1000) * 1000);

    auto res = cli.Delete(prefix + "/" + name);
    return res && res->status == 200;
}

bool HttpCloudSaveProvider::pushToCloud(const std::string& slotPath) {
    const std::string content = m_local->readFile(slotPath);
    if (content.empty()) return false;  // nothing local to push
    return httpPut(safeName(slotPath), content);
}

bool HttpCloudSaveProvider::pullFromCloud(const std::string& slotPath) {
    const std::string body = httpGet(safeName(slotPath));
    if (body.empty()) return false;  // 404 or offline
    return m_local->writeFile(slotPath, body);
}

} // namespace Caesura
