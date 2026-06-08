#include "AsyncLoader.h"
#include "AssetManager.h"
#include "ImageDecoder.h"
#include "../Core/JobSystem.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <memory>

namespace Caesura {

static bool isPathSafe(const std::string& path) {
    return !path.empty() && path.find("..") == std::string::npos;
}

AsyncLoader& AsyncLoader::instance() {
    static AsyncLoader s;
    return s;
}

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

    JobSystem::instance().waitIdle();

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

    std::vector<uint8_t> raw = AssetManager::instance().read(req.path);
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
    return result;
}

int AsyncLoader::enqueue(const std::string& path, const std::string& type) {
    if (!m_running) return -1;
    if (!isPathSafe(path)) {
        fprintf(stderr, "[AsyncLoader] Path rejected: %s\n", path.c_str());
        return -1;
    }

    if (m_pendingCount >= 16) {
        fprintf(stderr, "[AsyncLoader] Queue full (16 pending), rejecting: %s\n", path.c_str());
        return -1;
    }

    m_cancelRequested.store(false);

    int id = m_nextId.fetch_add(1);
    AsyncLoadRequest req{id, path, type};
    auto result = std::make_shared<CompletedLoad>();
    auto cancelled = std::make_shared<std::atomic<bool>>(false);

    JobSystem::instance().submit(
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
            std::lock_guard<std::mutex> lock(m_completeMutex);
            m_completed.push_back(std::move(*result));
        });

    m_pendingCount++;
    printf("[AsyncLoader] Enqueued #%d: %s (%s) [pending=%d]\n",
           id, path.c_str(), type.c_str(), m_pendingCount.load());
    return id;
}

void AsyncLoader::cancelAll() {
    m_cancelRequested.store(true);
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
        SDL_PushEvent(&event);
    }
    return true;
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
    SDL_PushEvent(&event);
}

} // namespace Caesura
