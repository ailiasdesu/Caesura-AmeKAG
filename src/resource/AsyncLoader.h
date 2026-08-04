#pragma once
#include "api/IAsyncLoader.h"
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>

namespace Caesura {

class AssetManager;

struct AsyncLoadRequest {
    int         id = 0;
    std::string path;
    std::string type;
};

// ============================================================================
// AsyncLoader -- implements IAsyncLoader
// ============================================================================

class AsyncLoader : public IAsyncLoader {
public:
    // Backward-compat alias (CompletedLoad now in IAsyncLoader)
    using CompletedLoad = Caesura::CompletedLoad;
public:
    explicit AsyncLoader(AssetManager* assetManager);
    ~AsyncLoader() override;
    AsyncLoader(const AsyncLoader&) = delete;
    AsyncLoader& operator=(const AsyncLoader&) = delete;

    void init() override;
    void shutdown() override;

    int  enqueue(const std::string& path, const std::string& type) override;
    void cancelAll() override;
    bool poll() override;
    std::vector<CompletedLoad> drainCompleted() override;

    int  pendingCount() const override { return m_pendingCount.load(); }
    bool isRunning()   const override { return m_running; }

private:
    CompletedLoad processRequest(const AsyncLoadRequest& req);
    void postCompleteEvent(int requestId, const std::string& path,
                           const std::vector<uint8_t>& data, bool success);

    AssetManager* m_assetManager = nullptr;
    std::atomic<bool> m_running{false};
    std::atomic<int>  m_pendingCount{0};
    std::atomic<int>  m_nextId{1};
    std::atomic<bool> m_cancelRequested{false};
    // Generation counter: cancelAll() increments it; jobs snapshot it at
    // enqueue and abort if it changed before they run -- fixing the race
    // where enqueue's reset of the boolean flag undid a cancelAll.
    std::atomic<uint64_t> m_cancelGeneration{0};

    std::mutex m_completeMutex;
    std::vector<CompletedLoad> m_completed;
};

} // namespace Caesura
