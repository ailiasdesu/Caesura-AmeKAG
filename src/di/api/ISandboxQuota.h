// ISandboxQuota - pure virtual interface for Lua sandbox resource quotas
// Concrete: SandboxQuotaImpl. Pattern: module api/ directory.
#pragma once

namespace Caesura {

class ISandboxQuota {
public:
    virtual ~ISandboxQuota() = default;

    virtual bool tryAlloc(const char* kind) = 0;
    virtual void release(const char* kind) = 0;
    virtual int  count(const char* kind) = 0;
    virtual int  maxLimit(const char* kind) = 0;
};

} // namespace Caesura
