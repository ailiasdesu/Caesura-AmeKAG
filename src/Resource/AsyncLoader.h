#pragma once
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <unordered_map>

namespace Caesura {

// -- AsyncLoadRequest -----------------------------------------------------
struct AsyncLoadRequest {
    int         id = 0;          // unique request ID
    std::string path;             // asset file path
    std::string type;             // "texture" | "audio"
};

// -- AsyncLoader ----------------------------------------------------------
// Single background thread for file I/O. Posts SDL events to main thread
// for bgfx/SoLoud resource creation (main-thread-only requirement).
//
// Queue limit: 16 pending requests (high watermark).
// Cancel: clears queue + stops in-flight loads (best-effort).

// Custom SDL event type for async load completion (pushed from main thread)
static constexpr uint32_t CAESURA_EVENT_ASYNC_LOAD = 0x8000;

class AsyncLoader {
public:
    static AsyncLoader& instance();

    AsyncLoader(const AsyncLoader&) = delete;
    AsyncLoader& operator=(const AsyncLoader&) = delete;

    // -- Lifecycle (main thread) ------------------------------------------
    void init();
    void shutdown();

    // -- Queue management (main thread) -----------------------------------
    // Enqueue a load request. Returns request id on success, -1 if queue full.
    int  enqueue(const std::string& path, const std::string& type);

    // Cancel all pending and in-flight loads. Best-effort.
    void cancelAll();

    // -- Polling (main thread) — returns true when a load completed --------
    // Called from main loop. Dispatches completion callbacks.
    bool poll();

    // -- Stats ------------------------------------------------------------
    int  pendingCount() const { return m_pendingCount.load(); }
    bool isRunning()   const { return m_running; }

    // -- Completed load result (public: Engine::processEvents() accesses it) --
    struct CompletedLoad {
        int    id;
        std::string path;
        std::vector<uint8_t> data;
        bool   success;
    };

private:
    AsyncLoader() = default;
    ~AsyncLoader();

    void workerLoop();
    void postCompleteEvent(int requestId, const std::string& path,
                           const std::vector<uint8_t>& data, bool success);

    std::atomic<bool> m_running{false};
    std::thread       m_worker;

    mutable std::mutex m_mutex;
    std::queue<AsyncLoadRequest> m_queue;
    std::atomic<int> m_pendingCount{0};
    std::atomic<int> m_nextId{1};
    std::atomic<bool> m_cancelRequested{false};

    std::mutex m_completeMutex;
    std::vector<CompletedLoad> m_completed;
};

} // namespace Caesura