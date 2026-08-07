#include "AsyncLoader.h"
#include "AssetManager.h"
#include "ImageDecoder.h"
#include "../di/BackendRegistry.h"
#include "../job/api/IJobSystem.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <memory>

namespace Caesura {

static bool isPathSafe(const std::string& path) {
    return !path.empty() && path.find("..") == std::string::npos;
}

AsyncLoader::AsyncLoader(AssetManager* assetManager)
    : m_assetManager(assetManager) {}

AsyncLoader::~AsyncLoader() {
    shutdown();
}

void AsyncLoader::init() {
    if (m_running) return;
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
        fprintf(stderr, "[AsyncLoader] AssetManager unavailable: %s\n", req.path.c_str());
        result.success = false;
        return result;
    }

    std::vector<uint8_t> raw = m_assetManager->read(req.path);
    if (raw.empty()) {
        fprintf(stderr, "[AsyncLoader] Asset not found: %s\n", req.path.c_str());
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
            fprintf(stderr, "[AsyncLoader] Decode failed: %s\n", req.path.c_str());
            result.success = false;
        }
    } else {
        result.data    = std::move(raw);
        result.success = true;
        printf("[AsyncLoader] Loaded #%d: %s (%zu bytes)\n",
               req.id, req.path.c_str(), result.data.size());
    }
    } catch (const std::exception& e) {
        fprintf(stderr, "[AsyncLoader] Exception loading %s: %s\n",
                req.path.c_str(), e.what());
        result.success = false;
    } catch (...) {
        fprintf(stderr, "[AsyncLoader] Unknown exception loading %s: %s\n",
                req.path.c_str(), "?");
        result.success = false;
    }
    return result;
}

int AsyncLoader::enqueue(const std::string& path, const std::string& type) {
    if (!m_running) return -1;
    if (!m_assetManager) {
        fprintf(stderr, "[AsyncLoader] AssetManager unavailable; rejecting: %s\n", path.c_str());
        return -1;
    }
    if (!isPathSafe(path)) {
        fprintf(stderr, "[AsyncLoader] Path rejected: %s\n", path.c_str());
        return -1;
    }

    if (m_pendingCount >= 16) {
        fprintf(stderr, "[AsyncLoader] Queue full (16 pending), rejecting: %s\n", path.c_str());
        return -1;
    }

    auto* jobSystem = BackendRegistry::instance().getJobSystem();
    if (!jobSystem || !jobSystem->isRunning()) {
        fprintf(stderr, "[AsyncLoader] JobSystem unavailable; rejecting: %s\n", path.c_str());
        return -1;
    }

    m_cancelRequested.store(false);

    // Cache hit: a previous successful decode of this (path, type) is
    // still resident -- complete immediately without touching the job
    // system (no IO, no decode, no worker round trip). Scene re-entry is
    // the common case in VNs (back to title, gallery thumbnails).
    const std::string cacheKey = path + "|" + type;
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
            printf("[AsyncLoader] Cache hit: %s (%s)\n", path.c_str(), type.c_str());
            return id;
        }
    }

    int id = m_nextId.fetch_add(1);
    AsyncLoadRequest req{id, path, type};
    auto result = std::make_shared<CompletedLoad>();
    auto cancelled = std::make_shared<std::atomic<bool>>(false);

    const uint64_t jobId = jobSystem->submit(
        [req, result, cancelled, this]() {
            if (m_cancelRequested.load()) {
                cancelled->store(true);
                return;
            }
            *result = processRequest(req);
        },
        JobPriority::Normal,
        [this, result, cancelled]() {
            if (cancelled->load()) {
                m_pendingCount--;
                return;
            }
            // Successful decode -> keep in the bounded cache (main thread).
            if (result->success && !result->rgba.empty()) {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                const std::string key = result->path + "|" + result->type;
                if (m_rgbaCache.find(key) == m_rgbaCache.end()) {
                    const size_t bytes = result->rgba.size()
                                       + result->data.size();
                    m_rgbaCache[key] = *result;
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
            std::lock_guard<std::mutex> lock(m_completeMutex);
            m_completed.push_back(std::move(*result));
        });

    if (jobId == 0) {
        fprintf(stderr, "[AsyncLoader] Job submission failed: %s\n", path.c_str());
        return -1;
    }

    m_pendingCount++;
    printf("[AsyncLoader] Enqueued #%d: %s (%s) [pending=%d]\n",
           id, path.c_str(), type.c_str(), m_pendingCount.load());
    return id;
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
    printf("[AsyncLoader] All loads cancelled.\n");
}

bool AsyncLoader::poll() {
    std::lock_guard<std::mutex> lock(m_completeMutex);
    if (m_completed.empty()) return false;

    auto completed = std::move(m_completed);
    m_completed.clear();

    for (auto& c : completed) {
        m_pendingCount--;
        SDL_Event event;
        SDL_zero(event);
        event.type = CAESURA_EVENT_ASYNC_LOAD;
        event.user.data1 = new CompletedLoad(std::move(c));
        if (SDL_PushEvent(&event) != 0) {
            // Event queue full: free the payload instead of leaking it.
            delete static_cast<CompletedLoad*>(event.user.data1);
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "[AsyncLoader] SDL_PushEvent failed: %s", SDL_GetError());
        }
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

void AsyncLoader::postCompleteEvent(int requestId, const std::string& path,
                                     const std::vector<uint8_t>& data, bool success) {
    auto* completed = new CompletedLoad{};
    completed->id = requestId;
    completed->path = path;
    completed->data = data;
    completed->success = success;
    SDL_Event event;
    SDL_zero(event);
    event.type = CAESURA_EVENT_ASYNC_LOAD;
    event.user.data1 = completed;
    if (SDL_PushEvent(&event) != 0) {
        delete completed;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[AsyncLoader] SDL_PushEvent failed: %s", SDL_GetError());
    }
}

} // namespace Caesura
