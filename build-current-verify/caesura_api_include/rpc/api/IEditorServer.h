// IEditorServer - pure virtual interface for web editor HTTP server
// Concrete: EditorServer. Pattern: module api/ directory.
#pragma once
#include "../../archive/api/IArchiveWriter.h"
#include "IRpcDispatcher.h"

#include <functional>
#include <memory>
#include <string>

namespace Caesura {

class IEditorServer {
public:
    using ArchiveWriterFactory = std::function<std::unique_ptr<carc::IArchiveWriter>()>;

    virtual ~IEditorServer() = default;

    virtual bool start(int port = 9876) = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual int  port() const = 0;
    virtual void pushLog(const std::string& level, const std::string& message) = 0;
    virtual void setDispatcher(std::shared_ptr<IRpcDispatcher> dispatcher) = 0;
    virtual void setWebRoot(const std::string& path) = 0;
    virtual void setArchiveWriterFactory(ArchiveWriterFactory factory) = 0;
};

} // namespace Caesura
