// IRpcServer - pure virtual interface for JSON-RPC server
// Concrete: RpcServer. Pattern: module api/ directory.
#pragma once
#include <string>
#include <functional>

namespace Caesura {

class IRpcServer {
public:
    virtual ~IRpcServer() = default;

    virtual void run() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual void setFrameCaptureCallback(std::function<std::string(int,int)> cb) = 0;
    virtual void pushLog(const std::string& level, const std::string& message) = 0;
};

} // namespace Caesura
