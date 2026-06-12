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
    virtual void shutdown() = 0;
    virtual uint64_t submit(JobFn work,
                            JobPriority priority = JobPriority::Normal,
                            MainThreadFn onComplete = nullptr) = 0;
    virtual void pollMainThreadJobs() = 0;
    virtual void waitIdle() = 0;
    virtual int  workerCount() const = 0;
    virtual int  pendingJobs() const = 0;
    virtual bool isRunning() const = 0;
};

} // namespace Caesura
