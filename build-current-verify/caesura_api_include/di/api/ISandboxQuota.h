// ISandboxQuota - pure virtual interface for Lua sandbox resource quotas
// Concrete: SandboxQuotaService. Pattern: module api/ directory.
#pragma once

struct lua_State;

namespace Caesura {

class ISandboxQuota {
public:
    virtual ~ISandboxQuota() = default;

    virtual void setLuaState(lua_State* L) = 0;
    virtual bool tryAlloc(const char* kind) = 0;
    virtual void release(const char* kind) = 0;
    virtual int  count(const char* kind) = 0;
    virtual int  maxLimit(const char* kind) = 0;
};

} // namespace Caesura
