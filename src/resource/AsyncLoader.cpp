#include "AsyncLoader.h"
#include "AssetManager.h"
#include "ImageDecoder.h"
#include "../debug/api/DebugLog.h"
#include "../di/BackendRegistry.h"
#include "../job/api/IJobSystem.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <memory>

namespace Caesura {

uint32_t CAESURA_EVENT_ASYNC_LOAD = 0;

static bool isPathSafe(const std::string& path) {
    return !path.empty() && path.find("..") == std::string::npos;
}

// Length-prefixed (path,type) composite key -- the "|"-free separator avoids
// any collision between, e.g. path="a|b",type="c" and path="a",type="b|c".
std::string AsyncLoader::makeKey(const std::string& path, const std::string& type) {
    return std::to_string(path.size()) + ":" + path + type;
}

AsyncLoader::AsyncLoader(AssetManager* assetManager)
    : m_assetManager(assetManager) {}

AsyncLoader::~AsyncLoader() {
    shutdown();
}

void AsyncLoader::init() {
    if (m_running) return;
    // SDL keeps registered user-event IDs for the process lifetime. Reserve
    // once on first use, without calling SDL during static initialization.
    static const uint32_t eventType = [] {
        CAESURA_EVENT_ASYNC_LOAD = SDL_RegisterEvents(1);
        return CAESURA_EVENT_ASYNC_LOAD;
    }();
    if (eventType == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "[AsyncLoader] No SDL user-event type available");
        return;
    }
    m_running = true;
    m_cancelRequested = false;
    printf("[AsyncLoader] Initialized via JobSystem (max 16 pending)\n");
}

void AsyncLoader::shutdown() {
    if (!m_running) return;
    m_running = false;
    m_cancelRequested = true;

    if (auto* jobSystem = BackendRegistry::instance().getJobSystem()) {
        jobSystem->waitIdle();
        jobSystem->pollMainThreadJobs();
    }

    {
        std::lock_guard<std::mutex> lock(m_completeMutex);
        m_completed.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_inflightMutex);
        m_inflight.clear();
    }
    // SDL owns only the shallow event copy, never user.data1. Events already
    // taken by Engine belong to that consumer; reclaim only this loader's
    // payloads still in the queue, leaving other owners' events untouched.
    if (SDL_WasInit(SDL_INIT_EVENTS)) {
        SDL_FilterEvents([](void* owner, SDL_Event* event) -> bool {
            if (event->type != CAESURA_EVENT_ASYNC_LOAD || event->user.data2 != owner) {
                return true;
            }
            delete static_cast<CompletedLoad*>(event->user.data1);
            return false;
        }, this);
    }
    m_pendingCount = 0;
    m_cancelRequested = false;
    printf("[AsyncLoader] Shutdown complete.\n");
}

AsyncLoader::CompletedLoad AsyncLoader::processRequest(const AsyncLoadRequest& req) {
    CompletedLoad result;
    result.id   = req.id;
    result.path = req.path;
    result.type = req.type;


    // Any decode/read exception (e.g. std::bad_alloc on a forged oversized
    // image header) would otherwise std::terminate the worker thread and
    // kill the process. Surface it as a failed load instead.
    try {
    if (!m_assetManager) {
        DEBUG_ERR(SubSys::Resource, ErrCode::Resource_LoadFailed,
            "[AsyncLoader] AssetManager unavailable: %s", req.path.c_str());
        result.success = false;
        return result;
    }

    std::vector<uint8_t> raw = m_assetManager->read(req.path);
    if (raw.empty()) {
        DEBUG_ERR(SubSys::Resource, ErrCode::Resource_LoadFailed,
            "[AsyncLoader] Asset not found: %s", req.path.c_str());
        result.success = false;
        return result;
    }

    if (req.type == "texture") {
        DecodedImage decoded = ImageDecoder::decode(raw.data(), raw.size());
        if (decoded.ok) {
            result.rgba    = std::move(decoded.rgba);
            result.width   = decoded.width;
            result.height  = decoded.height;
            result.success = true;
            printf("[AsyncLoader] Decoded #%d: %s (%ux%u)\n",
                   req.id, req.path.c_str(), result.width, result.height);
        } else {
            DEBUG_ERR(SubSys::Resource, ErrCode::Resource_DecodeFailed,
                "[AsyncLoader] Decode failed: %s", req.path.c_str());
            result.success = false;
        }
    } else {
        result.data    = std::move(raw);
        result.success = true;
        printf("[AsyncLoader] Loaded #%d: %s (%zu bytes)\n",
               req.id, req.path.c_str(), result.data.size());
    }
    } catch (const std::exception& e) {
        DEBUG_ERR(SubSys::Resource, ErrCode::Resource_LoadFailed,
            "[AsyncLoader] Exception loading %s: %s",
            req.path.c_str(), e.what());
        result.success = false;
    } catch (...) {
        DEBUG_ERR(SubSys::Resource, ErrCode::Resource_LoadFailed,
            "[AsyncLoader] Unknown exception loading %s: %s",
            req.path.c_str(), "?");
        result.success = false;
    }
    return result;
}

int AsyncLoader::enqueue(const std::string& path, const std::string& type) {
    if (!m_running) return -1;
    if (!m_assetManager) {
        DEBUG_ERR(SubSys::Resource, ErrCode::Ok,
            "[AsyncLoader] AssetManager unavailable; rejecting: %s", path.c_str());
        return -1;
    }
    if (!isPathSafe(path)) {
        DEBUG_ERR(SubSys::Resource, ErrCode::Ok,
            "[AsyncLoader] Path rejected: %s", path.c_str());
        return -1;
    }

    if (m_pendingCount >= 16) {
        DEBUG_ERR(SubSys::Resource, ErrCode::Ok,
            "[AsyncLoader] Queue full (16 pending), rejecting: %s", path.c_str());
        return -1;
    }

    auto* jobSystem = BackendRegistry::instance().getJobSystem();
    if (!jobSystem || !jobSystem->isRunning()) {
        DEBUG_ERR(SubSys::Resource, ErrCode::Ok,
            "[AsyncLoader] JobSystem unavailable; rejecting: %s", path.c_str());
        return -1;
    }

    m_cancelRequested.store(false);

    const std::string cacheKey = makeKey(path, type);

    // Cache hit: a previous successful decode of this (path, type) is
    // still resident -- complete immediately without touching the job
    // system (no IO, no decode, no worker round trip). Scene re-entry is
    // the common case in VNs (back to title, gallery thumbnails).
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_rgbaCache.find(cacheKey);
        if (it != m_rgbaCache.end()) {
            const int id = m_nextId.fetch_add(1);
            CompletedLoad hit = it->second;
            hit.id = id;
            {
                std::lock_guard<std::mutex> lock2(m_completeMutex);
                m_completed.push_back(std::move(hit));
            }
            // Cache hits enqueue into m_completed too, so account them
            // the same way as the job path -- poll()/drainCompleted() decrement
            // per consumed entry and would otherwise drift negative.
            m_pendingCount++;
            printf("[AsyncLoader] Cache hit: %s (%s) [pending=%d]\n",
                   path.c_str(), type.c_str(), m_pendingCount.load());
            return id;
        }
    }

    // In-flight dedup: the same (path,type) already has an unfinished load.
    // Share that single load instead of submitting a second job -- the source
    // is read once, never twice, for concurrent duplicates (round 90 gap).
    {
        std::lock_guard<std::mutex> lock(m_inflightMutex);
        auto it = m_inflight.find(cacheKey);
        if (it != m_inflight.end()) {
            const int id = m_nextId.fetch_add(1);
            it->second->waiterIds.push_back(id);
            m_pendingCount++;  // this waiter still owes a completion to poll()
            printf("[AsyncLoader] Shared in-flight #%d with %s (%s) [pending=%d %zu]\n",
                   id, path.c_str(), type.c_str(),
                   m_pendingCount.load(), it->second->waiterIds.size());
            return id;
        }
    }

    int id = m_nextId.fetch_add(1);
    AsyncLoadRequest req{id, path, type};

    auto entry = std::make_shared<InFlightEntry>();
    entry->result    = std::make_shared<CompletedLoad>();
    entry->cancelled = std::make_shared<std::atomic<bool>>(false);
    entry->waiterIds.push_back(id);

    // Register the in-flight entry BEFORE submitting: a second enqueue for the
    // same key (from this thread or, in tests, another) now shares the load.
    {
        std::lock_guard<std::mutex> lock(m_inflightMutex);
        m_inflight[cacheKey] = entry;
    }
    // The first submitter accounts for its own completion; a shared waiter
    // increments on top. Do it now so a submit failure can be unwound cleanly.
    m_pendingCount++;

    auto result    = entry->result;
    auto cancelled = entry->cancelled;

    const uint64_t jobId = jobSystem->submit(
        [req, result, cancelled, this]() {
            if (m_cancelRequested.load()) {
                cancelled->store(true);
                return;
            }
            *result = processRequest(req);
        },
        JobPriority::Normal,
        [this, cacheKey, entry, result]() {
            bool cancelled = entry->cancelled->load();
            // Release the in-flight entry (no-op if cancelAll already dropped it).
            {
                std::lock_guard<std::mutex> lock(m_inflightMutex);
                auto eit = m_inflight.find(cacheKey);
                if (eit != m_inflight.end() && eit->second == entry) {
                    m_inflight.erase(eit);
                }
            }
            finishInFlight(entry, cancelled);
        });

    if (jobId == 0) {
        // Submission failed: drop the registration, unwind pendingCount, reject.
        bool removed = false;
        {
            std::lock_guard<std::mutex> lock(m_inflightMutex);
            auto eit = m_inflight.find(cacheKey);
            if (eit != m_inflight.end() && eit->second == entry) {
                m_inflight.erase(eit);
                removed = true;
            }
        }
        if (removed) m_pendingCount--;
        DEBUG_ERR(SubSys::Resource, ErrCode::Ok,
            "[AsyncLoader] Job submission failed: %s", path.c_str());
        return -1;
    }

    printf("[AsyncLoader] Enqueued #%d: %s (%s) [pending=%d]\n",
           id, path.c_str(), type.c_str(), m_pendingCount.load());
    return id;
}

void AsyncLoader::finishInFlight(const std::shared_ptr<InFlightEntry>& entry,
                                 bool cancelled) {
    // Must run on the main thread (real JobSystem) or synchronously on the
    // submitting thread (NullJobSystem). Collect the waiters that shared this
    // load, then discharge every one with a CompletedLoad so pendingCount
    // drains and drainCompleted() sees a terminal entry per enqueue.
    if (!entry || entry->waiterIds.empty()) return;

    std::vector<int> waiters = std::move(entry->waiterIds);

    if (cancelled) {
        // Pre-empted by cancelAll while queued: deliver a terminal (failed)
        // result to every waiter so counters stay balanced and callers observe
        // the load ending. Failed loads are not cached, so a later enqueue
        // retries the source (retry is allowed by design).
        std::lock_guard<std::mutex> lock(m_completeMutex);
        for (int wid : waiters) {
            CompletedLoad failed;
            failed.id     = wid;
            failed.path   = entry->result->path;
            failed.type   = entry->result->type;
            failed.success = false;
            m_completed.push_back(std::move(failed));
        }
        // pendingCount is decremented only when drainCompleted()/poll() consume
        // these entries, matching the synchronous-job cap accounting.
        return;
    }

    const CompletedLoad& r = *entry->result;

    // Successful decode -> keep in the bounded cache (main thread).
    if (r.success && !r.rgba.empty()) {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        const std::string key = makeKey(r.path, r.type);
        if (m_rgbaCache.find(key) == m_rgbaCache.end()) {
            const size_t bytes = r.rgba.size() + r.data.size();
            m_rgbaCache[key] = r;
            m_cacheOrder.push_back(key);
            m_cacheBytes += bytes;
            // FIFO eviction by bytes then entry count.
            while ((m_cacheBytes > kCacheLimitBytes
                    || m_cacheOrder.size() > kCacheMaxEntries)
                   && !m_cacheOrder.empty()) {
                const std::string victim = m_cacheOrder.front();
                m_cacheOrder.erase(m_cacheOrder.begin());
                auto vit = m_rgbaCache.find(victim);
                if (vit != m_rgbaCache.end()) {
                    m_cacheBytes -= vit->second.rgba.size()
                                  + vit->second.data.size();
                    m_rgbaCache.erase(vit);
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_completeMutex);
        for (int wid : waiters) {
            CompletedLoad c = r;
            c.id = wid;
            m_completed.push_back(std::move(c));
        }
    }
    // pendingCount is decremented only when drainCompleted()/poll() consume
    // these entries (one per waiter), matching the synchronous-job cap.
}

void AsyncLoader::cancelAll() {
    m_cancelRequested.store(true);
    // The cache contract mirrors cancelAll: everything in flight is
    // invalidated, so cached decodes go too (hot-reload-safe).
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_rgbaCache.clear();
        m_cacheOrder.clear();
        m_cacheBytes = 0;
    }
    // Drop in-flight dedup associations: a fresh enqueue of the same key starts
    // a brand-new load. Jobs that were already running still hold their own
    // shared_ptr<InFlightEntry> and discharge their waiters on completion.
    {
        std::lock_guard<std::mutex> lock(m_inflightMutex);
        m_inflight.clear();
    }
    printf("[AsyncLoader] All loads cancelled.\n");
}

bool AsyncLoader::poll() {
    // Release the completion mutex before invoking SDL filters/watchers, which
    // may themselves query the loader. Both delivery paths share accounting.
    auto completed = drainCompleted();
    if (completed.empty()) return false;

    for (auto& c : completed) {
        postCompleteEvent(std::move(c));
    }
    return true;
}

std::vector<AsyncLoader::CompletedLoad> AsyncLoader::drainCompleted() {
    std::lock_guard<std::mutex> lock(m_completeMutex);
    std::vector<CompletedLoad> out;
    out.swap(m_completed);
    m_pendingCount -= static_cast<int>(out.size());
    return out;
}

void AsyncLoader::postCompleteEvent(CompletedLoad completed) {
    auto payload = std::make_unique<CompletedLoad>(std::move(completed));
    SDL_Event event{};
    event.type = CAESURA_EVENT_ASYNC_LOAD;
    event.user.data1 = payload.get();
    event.user.data2 = this;
    if (SDL_PushEvent(&event)) {
        payload.release(); // queue consumer (or shutdown) owns it now
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[AsyncLoader] SDL event rejected or push failed: %s", SDL_GetError());
    }
}

} // namespace Caesura
