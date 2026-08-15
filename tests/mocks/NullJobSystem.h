#pragma once
#include "../../src/job/api/IJobSystem.h"
#include <cstdio>
#include <cstdint>
#include <exception>

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
        // Exception isolation mirrors the real JobSystem: a throwing task or
        // completion callback is caught, reported, and swallowed so it can
        // neither escape the submit() caller nor corrupt the queue. The
        // completion callback still runs, and later submissions are unaffected.
        if (work) {
            try {
                work();
            } catch (const std::exception& e) {
                fprintf(stderr,
                        "[NullJobSystem] submit work() threw -- isolated and swallowed: %s\n",
                        e.what());
            } catch (...) {
                fprintf(stderr,
                        "[NullJobSystem] submit work() threw unknown exception -- "
                        "isolated and swallowed\n");
            }
        }
        if (onComplete) {
            try {
                onComplete();
            } catch (const std::exception& e) {
                fprintf(stderr,
                        "[NullJobSystem] submit onComplete() threw -- isolated and "
                        "swallowed: %s\n",
                        e.what());
            } catch (...) {
                fprintf(stderr,
                        "[NullJobSystem] submit onComplete() threw unknown exception -- "
                        "isolated and swallowed\n");
            }
        }
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
