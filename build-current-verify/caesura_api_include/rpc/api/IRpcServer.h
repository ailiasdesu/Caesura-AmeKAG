// IRpcServer - pure virtual interface for JSON-RPC server
// Concrete: RpcServer. Pattern: module api/ directory.
#pragma once
#include "IRpcDispatcher.h"

#include <memory>
#include <string>

namespace Caesura {

class IRpcServer {
public:
    virtual ~IRpcServer() = default;

    virtual void run() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual void setDispatcher(std::shared_ptr<IRpcDispatcher> dispatcher) = 0;
    virtual void pushLog(const std::string& level, const std::string& message) = 0;
};

} // namespace Caesura
