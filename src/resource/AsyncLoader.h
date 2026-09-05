#pragma once
#include "api/IAsyncLoader.h"
#include <cstdint>  // fixed-width types (GCC strict)
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>
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
    // A load currently in flight for one (path,type) key. Every enqueue that
    // targets a key with a live InFlightEntry shares this single load instead
    // of submitting its own job, so the source is read exactly once even when
    // the same asset is requested concurrently (round 90 gap: dedup only via
    // the completion cache could not collapse two truly-parallel requests).
    struct InFlightEntry {
        std::vector<int>  waiterIds;                     // every enqueue id sharing this load
        std::shared_ptr<CompletedLoad>   result;         // filled by the worker thread
        std::shared_ptr<std::atomic<bool>> cancelled;    // set when the job was pre-empted
    };

    static std::string makeKey(const std::string& path, const std::string& type);

    CompletedLoad processRequest(const AsyncLoadRequest& req);
    void postCompleteEvent(CompletedLoad completed);
    // Runs on the main thread (or synchronously under NullJobSystem) when a
    // shared in-flight load finishes: releases the entry and delivers one
    // CompletedLoad (+ pendingCount accounting) to every registered waiter.
    void finishInFlight(const std::shared_ptr<InFlightEntry>& entry,
                        bool cancelled);

    AssetManager* m_assetManager = nullptr;
    std::atomic<bool> m_running{false};
    std::atomic<int>  m_pendingCount{0};
    std::atomic<int>  m_nextId{1};
    std::atomic<bool> m_cancelRequested{false};

    std::mutex m_completeMutex;
    std::vector<CompletedLoad> m_completed;

    // Per-(path,type) in-flight dedup table. Guarded by m_inflightMutex; the
    // engine touches it from the main thread, tests may use several. cancelAll
    // clears the map (a later enqueue starts a fresh load); a running job still
    // holds its own shared_ptr<InFlightEntry>, so its waiters are discharged
    // correctly even after the map entry is dropped.
    std::mutex m_inflightMutex;
    std::unordered_map<std::string, std::shared_ptr<InFlightEntry>> m_inflight;

    // Decoded-resource cache (modern resource pipeline): successful loads
    // are kept (bounded by total bytes) so re-entering the same scene does
    // not re-read + re-decode the same files. cancelAll() clears it (its
    // contract is "invalidate everything"). Guarded by m_cacheMutex; the
    // engine touches it from the main thread, tests may use several.
    std::mutex m_cacheMutex;
    std::unordered_map<std::string, CompletedLoad> m_rgbaCache;
    std::vector<std::string> m_cacheOrder;  // FIFO eviction order
    size_t m_cacheBytes = 0;
    static constexpr size_t kCacheLimitBytes = 96 * 1024 * 1024;
    static constexpr size_t kCacheMaxEntries = 512;
};

} // namespace Caesura
