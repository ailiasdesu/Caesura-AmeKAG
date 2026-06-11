#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>

namespace Caesura {

// -- AsyncLoadRequest -----------------------------------------------------
struct AsyncLoadRequest {
    int         id = 0;
    std::string path;
    std::string type;  // "texture" | "audio"
};

// -- AsyncLoader ----------------------------------------------------------
// Submits asset I/O + CPU decode jobs to JobSystem (green zone).
// Main-thread poll() dispatches SDL events for GPU upload (red zone).
//
// Queue limit: 16 pending requests (high watermark).
// Cancel: clears counter + stops in-flight loads (best-effort).

static constexpr uint32_t CAESURA_EVENT_ASYNC_LOAD = 0x8000;

class AsyncLoader {
public:
    static AsyncLoader& instance();

    AsyncLoader(const AsyncLoader&) = delete;
    AsyncLoader& operator=(const AsyncLoader&) = delete;

    void init();
    void shutdown();

    int  enqueue(const std::string& path, const std::string& type);
    void cancelAll();

    bool poll();

    int  pendingCount() const { return m_pendingCount.load(); }
    bool isRunning()   const { return m_running; }

    struct CompletedLoad {
        int         id = 0;
        std::string path;
        std::string type;
        bool        success = false;
        std::vector<uint8_t> rgba;
        uint16_t    width  = 0;
        uint16_t    height = 0;
        std::vector<uint8_t> data;
    };

private:
    AsyncLoader() = default;
    ~AsyncLoader();

    static CompletedLoad processRequest(const AsyncLoadRequest& req);
    void postCompleteEvent(int requestId, const std::string& path,
                           const std::vector<uint8_t>& data, bool success);

    std::atomic<bool> m_running{false};
    std::atomic<int>  m_pendingCount{0};
    std::atomic<int>  m_nextId{1};
    std::atomic<bool> m_cancelRequested{false};

    std::mutex m_completeMutex;
    std::vector<CompletedLoad> m_completed;
};

} // namespace Caesura
