// IEditorServer - pure virtual interface for web editor HTTP server
// Concrete: EditorServer. Pattern: module api/ directory.
#pragma once
#include <string>

struct lua_State;

namespace Caesura {

class IEditorServer {
public:
    virtual ~IEditorServer() = default;

    virtual bool start(int port = 9876) = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual int  port() const = 0;
    virtual void pushLog(const std::string& level, const std::string& message) = 0;
    virtual void setLuaState(lua_State* L) = 0;
    virtual void setWebRoot(const std::string& path) = 0;
};

} // namespace Caesura
