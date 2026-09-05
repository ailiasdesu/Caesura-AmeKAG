// HttpCloudSaveProvider.cpp — see header. Uses httplib (header-only,
// exposed to every module via CaesarBuildOptions).
#include "HttpCloudSaveProvider.h"
#include "LocalFileSaveProvider.h"
#include <httplib.h>
#include <cstdio>
#include <memory>

namespace Caesura {

HttpCloudSaveProvider::HttpCloudSaveProvider(std::string endpoint, int timeoutMs,
                                                   std::string bearerToken)
    : m_endpoint(std::move(endpoint))
    , m_timeoutMs(timeoutMs > 0 ? timeoutMs : 8000)
    , m_bearerToken(std::move(bearerToken))
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
// Split "{scheme}://host:port/prefix" into (host, port, prefix, tls).
// Returns false on malformed input. Both http:// and https:// are accepted
// (ST-2); https selects httplib::SSLClient and defaults to port 443.
bool splitEndpoint(const std::string& endpoint, std::string& host,
                   int& port, std::string& prefix, bool& tls) {
    host = endpoint;
    tls = false;
    const std::string schemeHttp = "http://";
    const std::string schemeHttps = "https://";
    if (host.rfind(schemeHttps, 0) == 0) {
        tls = true;
        host = host.substr(schemeHttps.size());
    } else if (host.rfind(schemeHttp, 0) == 0) {
        host = host.substr(schemeHttp.size());
    } else {
        return false;
    }
    auto slash = host.find('/');
    if (slash != std::string::npos) {
        prefix = host.substr(slash);
        host = host.substr(0, slash);
    }
    port = tls ? 443 : 80;
    auto colon = host.rfind(':');
    if (colon != std::string::npos) {
        port = std::atoi(host.substr(colon + 1).c_str());
        host = host.substr(0, colon);
    }
    return !host.empty() && port > 0;
}

// Max accepted payload from cloud pulls (ST-2): mirrors the local MAX_SAVE_SIZE
// guard so a hostile/misconfigured server cannot exhaust disk via
// pullFromCloud's direct local write.
constexpr size_t kMaxCloudPayload = 10u * 1024u * 1024u;

// Build an httplib client for the parsed endpoint, applying TLS, timeouts and
// the optional bearer token (ST-2).
std::unique_ptr<httplib::Client> makeClient(const std::string& endpoint,
                                            int timeoutMs,
                                            const std::string& bearer) {
    std::string host, prefix;
    int port = 0;
    bool tls = false;
    if (!splitEndpoint(endpoint, host, port, prefix, tls)) return nullptr;

    std::unique_ptr<httplib::Client> cli;
    if (tls) {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        cli = std::make_unique<httplib::SSLClient>(host, port);
#else
        // No OpenSSL linked: an https endpoint must fail closed rather than
        // silently downgrading to plaintext (ST-2).
        return nullptr;
#endif
    } else {
        cli = std::make_unique<httplib::Client>(host, port);
    }
    cli->set_connection_timeout(2, 0);
    cli->set_read_timeout(timeoutMs / 1000, (timeoutMs % 1000) * 1000);
    cli->set_write_timeout(5, 0);
    if (!bearer.empty()) {
        cli->set_default_headers({{"Authorization", "Bearer " + bearer}});
    }
    return cli;
}

} // namespace

bool HttpCloudSaveProvider::httpPut(const std::string& name,
                                    const std::string& body) {
    if (m_endpoint.empty()) return false;
    std::string host, prefix;
    int port = 0;
    bool tls = false;
    if (!splitEndpoint(m_endpoint, host, port, prefix, tls)) return false;

    auto cli = makeClient(m_endpoint, m_timeoutMs, m_bearerToken);
    if (!cli) return false;

    auto res = cli->Put(prefix + "/" + name, body, "application/octet-stream");
    return res && res->status == 200;
}

std::string HttpCloudSaveProvider::httpGet(const std::string& name) {
    if (m_endpoint.empty()) return std::string();
    std::string host, prefix;
    int port = 0;
    bool tls = false;
    if (!splitEndpoint(m_endpoint, host, port, prefix, tls)) return std::string();

    auto cli = makeClient(m_endpoint, m_timeoutMs, m_bearerToken);
    if (!cli) return std::string();

    auto res = cli->Get(prefix + "/" + name);
    if (!res || res->status != 200) return std::string();
    if (res->body.size() > kMaxCloudPayload) return std::string();  // ST-2
    return res->body;
}

bool HttpCloudSaveProvider::httpDelete(const std::string& name) {
    if (m_endpoint.empty()) return false;
    std::string host, prefix;
    int port = 0;
    bool tls = false;
    if (!splitEndpoint(m_endpoint, host, port, prefix, tls)) return false;

    auto cli = makeClient(m_endpoint, m_timeoutMs, m_bearerToken);
    if (!cli) return false;

    auto res = cli->Delete(prefix + "/" + name);
    return res && res->status == 200;
}

bool HttpCloudSaveProvider::pushToCloud(const std::string& slotPath) {
    const std::string content = readLocalFile(slotPath);
    if (content.empty()) return false;  // nothing local to push
    return writeCloudFile(slotPath, content);
}

bool HttpCloudSaveProvider::pullFromCloud(const std::string& slotPath) {
    const std::string body = readCloudFile(slotPath);
    if (body.empty()) return false;  // 404 or offline
    return writeLocalFile(slotPath, body);
}

std::string HttpCloudSaveProvider::readLocalFile(const std::string& slotPath) {
    return m_local->readFile(slotPath);
}

bool HttpCloudSaveProvider::writeLocalFile(const std::string& slotPath, const std::string& bytes) {
    return m_local->writeFile(slotPath, bytes);
}

std::string HttpCloudSaveProvider::readCloudFile(const std::string& slotPath) {
    return httpGet(safeName(slotPath));
}

bool HttpCloudSaveProvider::writeCloudFile(const std::string& slotPath, const std::string& bytes) {
    return httpPut(safeName(slotPath), bytes);
}

} // namespace Caesura
