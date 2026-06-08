#include "JobSystem.h"
#include "Engine.h"
#include <cstdio>
#include <chrono>
#ifdef _WIN32\n#include <windows.h>\n#endif

namespace Caesura {

JobSystem& JobSystem::instance() {
    static JobSystem s;
    return s;
}

JobSystem::~JobSystem() {
    shutdown();
}

int JobSystem::computeWorkerCount() {
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 4;
    if (hw < 4) return 1;
    return static_cast<int>(hw) - 1;
}

void JobSystem::WorkQueue::push(Job job) {
    std::lock_guard<std::mutex> lock(mutex);
    if (job.priority == JobPriority::High)
        jobs.push_front(std::move(job));
    else
        jobs.push_back(std::move(job));
}

bool JobSystem::WorkQueue::pop(Job& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (jobs.empty()) return false;
    out = std::move(jobs.back());
    jobs.pop_back();
    return true;
}

bool JobSystem::WorkQueue::steal(Job& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (jobs.empty()) return false;
    out = std::move(jobs.front());
    jobs.pop_front();
    return true;
}

bool JobSystem::WorkQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex);
    return jobs.empty();
}

void JobSystem::init() {
    CAESURA_ASSERT_MAIN_THREAD();
    if (m_running.exchange(true)) return;

    m_workerCount = computeWorkerCount();
    m_queues.reserve(static_cast<size_t>(m_workerCount));
    for (int i = 0; i < m_workerCount; ++i) m_queues.emplace_back(std::make_unique<WorkQueue>());
    m_workers.reserve(static_cast<size_t>(m_workerCount));
    m_workerThreadIds.reserve(static_cast<size_t>(m_workerCount));

    for (int i = 0; i < m_workerCount; ++i) {
        m_workers.emplace_back(&JobSystem::workerLoop, this, i);
    }

    for (auto& t : m_workers) {
        m_workerThreadIds.push_back(t.get_id());
    }

    printf("[JobSystem] Initialized: %d worker(s) (hw=%u)\n",
           m_workerCount, std::thread::hardware_concurrency());
}

void JobSystem::shutdown() {
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_running.exchange(false)) return;

    waitIdle();
    notifyWorkers();

    // Join workers with timeout -- prevent indefinite hang on stuck workers.
    // Uses native Windows handle for WaitForSingleObject with 500ms deadline;
    // falls back to detach with warning if a worker doesn't stop in time.
    constexpr DWORD kTimeoutMs = 500;
    for (auto& t : m_workers) {
        if (!t.joinable()) continue;
        DWORD rc = WaitForSingleObject(t.native_handle(), kTimeoutMs);
        if (rc == WAIT_OBJECT_0) {
            t.join();  // immediately returns since thread already exited
        } else {
            t.detach();
            fprintf(stderr, "[JobSystem] Worker %u timed out after %lums -- detached\n",
                    GetThreadId(t.native_handle()), kTimeoutMs);
        }
    }
    m_workers.clear();
    m_workerThreadIds.clear();
    m_queues.clear();
    m_workerCount = 0;

    {
        std::lock_guard<std::mutex> lock(m_mainMutex);
        m_mainJobs.clear();
    }

    printf("[JobSystem] Shutdown complete.\n");
}

uint64_t JobSystem::submit(JobFn work, JobPriority priority, MainThreadFn onComplete) {
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_running || !work) return 0;

    uint64_t id = m_nextJobId.fetch_add(1);
    m_pendingJobs++;

    Job job;
    job.work       = std::move(work);
    job.onComplete = std::move(onComplete);
    job.priority   = priority;

    uint32_t idx = m_roundRobin.fetch_add(1) % static_cast<uint32_t>(m_workerCount);
    m_queues[idx]->push(std::move(job));
    notifyWorkers();
    return id;
}

void JobSystem::enqueueMainThreadJob(MainThreadFn fn) {
    if (!fn) return;
    std::lock_guard<std::mutex> lock(m_mainMutex);
    m_mainJobs.push_back(std::move(fn));
}

void JobSystem::pollMainThreadJobs() {
    CAESURA_ASSERT_MAIN_THREAD();

    std::deque<MainThreadFn> batch;
    {
        std::lock_guard<std::mutex> lock(m_mainMutex);
        batch.swap(m_mainJobs);
    }

    for (auto& fn : batch) {
        if (fn) fn();
    }
}

void JobSystem::waitIdle() {
    CAESURA_ASSERT_MAIN_THREAD();
    for (int spin = 0; spin < 5000 && m_pendingJobs.load() > 0; ++spin) {
        pollMainThreadJobs();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

bool JobSystem::isWorkerThread() const {
    auto id = std::this_thread::get_id();
    for (const auto& w : m_workerThreadIds) {
        if (w == id) return true;
    }
    return false;
}

void JobSystem::notifyWorkers() {
    m_cv.notify_all();
}

bool JobSystem::tryDequeueJob(int workerIndex, Job& out) {
    if (m_queues[static_cast<size_t>(workerIndex)]->pop(out))
        return true;

    for (int i = 0; i < m_workerCount; ++i) {
        if (i == workerIndex) continue;
        if (m_queues[static_cast<size_t>(i)]->steal(out))
            return true;
    }
    return false;
}

void JobSystem::workerLoop(int workerIndex) {
    printf("[JobSystem] Worker %d started.\n", workerIndex);

    while (m_running.load() || m_pendingJobs.load() > 0) {
        Job job;
        if (!tryDequeueJob(workerIndex, job)) {
            std::unique_lock<std::mutex> lock(m_waitMutex);
            m_cv.wait_for(lock, std::chrono::milliseconds(10), [this] {
                if (!m_running.load() && m_pendingJobs.load() == 0) return true;
                for (const auto& q : m_queues) {
                    if (!q->empty()) return true;
                }
                return false;
            });
            continue;
        }

        if (job.work) {
            job.work();
        }

        if (job.onComplete) {
            enqueueMainThreadJob(std::move(job.onComplete));
        }

        m_pendingJobs--;
        if (m_pendingJobs.load() == 0) {
            notifyWorkers();
        }
    }

    printf("[JobSystem] Worker %d stopped.\n", workerIndex);
}

} // namespace Caesura
