// IJobSystem - pure virtual interface for parallel job execution
// Concrete: JobSystem. Pattern: module api/ directory.
#pragma once
#include <functional>
#include <cstdint>

namespace Caesura {

enum class JobPriority : uint8_t {
    High   = 0,
    Normal = 1,
    Low    = 2,
};

using JobFn        = std::function<void()>;
using MainThreadFn = std::function<void()>;

class IJobSystem {
public:
    virtual ~IJobSystem() = default;

    virtual void init() = 0;
    // Stop admission, join accepted workers, then deliver one final completion
    // snapshot while backend owners are still alive. Reentrant shutdown from
    // a callback cancels remaining callbacks; init is ignored during shutdown.
    virtual void shutdown() = 0;
    virtual uint64_t submit(JobFn work,
                            JobPriority priority = JobPriority::Normal,
                            MainThreadFn onComplete = nullptr) = 0;
    // Main thread only. Process one ready snapshot; callbacks may submit work
    // for a later poll. Nested polls do not recursively drain more callbacks.
    virtual void pollMainThreadJobs() = 0;
    // Main thread only. Wait for accepted worker bodies and publication of
    // their completion callbacks, but do not execute those callbacks. Workers
    // must not synchronously wait for a main-thread callback to finish.
    virtual void waitIdle() = 0;
    virtual int  workerCount() const = 0;
    virtual int  pendingJobs() const = 0;
    virtual bool isRunning() const = 0;
};

} // namespace Caesura
