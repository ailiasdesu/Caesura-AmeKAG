#pragma once
#include "api/IJobSystem.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace Caesura {


// Fiber-free work-stealing job system (Beta Part 8 subset).
// - Workers execute JobFn (green zone: I/O, decode, CPU math).
// - MainThreadFn callbacks are queued and run via pollMainThreadJobs() (red zone).
class JobSystem : public IJobSystem {
public:
    static JobSystem& instance();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    // Lifecycle (main thread only)
    void init();
    void shutdown();

    // Submit CPU work. Returns opaque job id (0 on failure).
    // onComplete, if set, runs on main thread after work finishes.
    uint64_t submit(JobFn work,
                    JobPriority priority = JobPriority::Normal,
                    MainThreadFn onComplete = nullptr);

    // Drain main-thread callback queue (call once per frame from Engine).
    void pollMainThreadJobs();

    // Block until all submitted jobs finish (main thread; used during shutdown).
    void waitIdle();

    int  workerCount() const { return m_workerCount; }
    int  pendingJobs() const { return m_pendingJobs.load(); }
    bool isRunning()   const { return m_running.load(); }
    bool isWorkerThread() const;

private:
    JobSystem() = default;
    ~JobSystem();

    struct Job {
        JobFn          work;
        MainThreadFn   onComplete;
        JobPriority    priority = JobPriority::Normal;
    };

    struct WorkQueue {
        std::deque<Job> jobs;
        mutable std::mutex mutex;

        void push(Job job);
        bool pop(Job& out);    // owner: take from back (LIFO)
        bool steal(Job& out);  // thief: take from front
        bool empty() const;
    };

    static int computeWorkerCount();

    void workerLoop(int workerIndex);
    bool tryDequeueJob(int workerIndex, Job& out);
    void enqueueMainThreadJob(MainThreadFn fn);
    void notifyWorkers();

    std::atomic<bool> m_running{false};
    int m_workerCount = 0;

    std::vector<std::unique_ptr<WorkQueue>> m_queues;
    std::vector<std::thread>            m_workers;
    std::vector<std::thread::id>        m_workerThreadIds;

    std::mutex              m_mainMutex;
    std::deque<MainThreadFn> m_mainJobs;

    std::mutex              m_waitMutex;
    std::condition_variable m_cv;

    std::atomic<uint64_t> m_nextJobId{1};
    std::atomic<int>      m_pendingJobs{0};
    std::atomic<uint32_t> m_roundRobin{0};
};

} // namespace Caesura
