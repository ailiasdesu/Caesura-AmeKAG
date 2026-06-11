#pragma once
#include "api/IAsyncLoader.h"
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>

namespace Caesura {

struct AsyncLoadRequest {
    int         id = 0;
    std::string path;
    std::string type;
};

static constexpr uint32_t CAESURA_EVENT_ASYNC_LOAD = 0x8000;

// ============================================================================
// AsyncLoader -- implements IAsyncLoader
// ============================================================================

class AsyncLoader : public IAsyncLoader {
public:
    // Backward-compat alias (CompletedLoad now in IAsyncLoader)
    using CompletedLoad = Caesura::CompletedLoad;
public:
    static AsyncLoader& instance();

    AsyncLoader(const AsyncLoader&) = delete;
    AsyncLoader& operator=(const AsyncLoader&) = delete;

    void init() override;
    void shutdown() override;

    int  enqueue(const std::string& path, const std::string& type) override;
    void cancelAll() override;
    bool poll() override;

    int  pendingCount() const override { return m_pendingCount.load(); }
    bool isRunning()   const override { return m_running; }

private:
    AsyncLoader() = default;
    ~AsyncLoader() override;

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