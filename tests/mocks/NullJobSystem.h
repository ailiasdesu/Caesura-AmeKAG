#pragma once
#include "../../src/job/api/IJobSystem.h"
#include <cstdint>

namespace Caesura {

// NullJobSystem -- synchronous mock for unit testing
// All submitted work runs immediately on the calling thread.
// No worker threads, no queues, no waiting.
// Use in tests to isolate modules from real JobSystem dependency.

class NullJobSystem : public IJobSystem {
public:
    NullJobSystem() = default;
    ~NullJobSystem() override = default;

    void init() override {
        m_running = true;
    }

    void shutdown() override {
        m_running = false;
        m_jobId = 1;
    }

    uint64_t submit(JobFn work,
                    JobPriority priority = JobPriority::Normal,
                    MainThreadFn onComplete = nullptr) override {
        (void)priority;
        if (!m_running) return 0;
        uint64_t id = m_jobId++;
        if (work) work();
        if (onComplete) onComplete();
        return id;
    }

    void pollMainThreadJobs() override {
        // No-op: all jobs already completed synchronously
    }

    void waitIdle() override {
        // No-op: synchronous mode, nothing pending
    }

    int workerCount() const override { return 0; }
    int pendingJobs() const override { return 0; }
    bool isRunning() const override { return m_running; }

private:
    bool m_running = false;
    uint64_t m_jobId = 1;
};

} // namespace Caesura
