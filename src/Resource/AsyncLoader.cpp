#include "AsyncLoader.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <fstream>
#include <chrono>

namespace Caesura {

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
    m_worker = std::thread(&AsyncLoader::workerLoop, this);
    printf("[AsyncLoader] Initialized (max 16 pending)\n");
}

void AsyncLoader::shutdown() {
    if (!m_running) return;
    m_running = false;
    m_cancelRequested = true;

    if (m_worker.joinable()) {
        m_worker.join();
    }

    // Clear queues
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_queue.empty()) m_queue.pop();
    }
    {
        std::lock_guard<std::mutex> lock(m_completeMutex);
        m_completed.clear();
    }
    m_pendingCount = 0;
    printf("[AsyncLoader] Shutdown complete.\n");
}

int AsyncLoader::enqueue(const std::string& path, const std::string& type) {
    if (!m_running) return -1;

    if (m_pendingCount >= 16) {
        fprintf(stderr, "[AsyncLoader] Queue full (16 pending), rejecting: %s\n", path.c_str());
        return -1;
    }

    int id = m_nextId.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push({id, path, type});
    }
    m_pendingCount++;
    printf("[AsyncLoader] Enqueued #%d: %s (%s) [pending=%d]\n",
           id, path.c_str(), type.c_str(), m_pendingCount.load());
    return id;
}

void AsyncLoader::cancelAll() {
    m_cancelRequested = true;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_queue.empty()) m_queue.pop();
    }
    m_pendingCount = 0;
    printf("[AsyncLoader] All loads cancelled.\n");
    m_cancelRequested = false;
}

bool AsyncLoader::poll() {
    std::lock_guard<std::mutex> lock(m_completeMutex);
    if (m_completed.empty()) return false;

    auto completed = std::move(m_completed);
    m_completed.clear();

    for (auto& c : completed) {
        m_pendingCount--;
        // Post SDL event for main-thread callback
        SDL_Event event;
        SDL_zero(event);
        event.type = CAESURA_EVENT_ASYNC_LOAD;
        event.user.data1 = new CompletedLoad(std::move(c));
        SDL_PushEvent(&event);
    }
    return true;
}

void AsyncLoader::workerLoop() {
    printf("[AsyncLoader] Worker thread started.\n");

    while (m_running) {
        AsyncLoadRequest req;
        bool hasWork = false;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_queue.empty()) {
                req = m_queue.front();
                m_queue.pop();
                hasWork = true;
            }
        }

        if (!hasWork) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (m_cancelRequested) continue;

        // Load file from disk
        std::vector<uint8_t> data;
        bool success = false;

        std::ifstream file(req.path, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);
            data.resize(size);
            file.read(reinterpret_cast<char*>(data.data()), size);
            success = file.good();
            printf("[AsyncLoader] Loaded #%d: %s (%zu bytes)\n",
                   req.id, req.path.c_str(), size);
        } else {
            fprintf(stderr, "[AsyncLoader] Failed to open: %s\n", req.path.c_str());
        }

                // Add to completed queue -- main-thread poll() will push SDL events
        {
            std::lock_guard<std::mutex> lock(m_completeMutex);
            m_completed.push_back({req.id, req.path, std::move(data), success});
        }
    }

    printf("[AsyncLoader] Worker thread stopped.\n");
}

void AsyncLoader::postCompleteEvent(int requestId, const std::string& path,
                                     const std::vector<uint8_t>& data, bool success) {
    auto* completed = new CompletedLoad{requestId, path, data, success};
    SDL_Event event;
    SDL_zero(event);
    event.type = CAESURA_EVENT_ASYNC_LOAD;
    event.user.data1 = completed;
    SDL_PushEvent(&event);
}

} // namespace Caesura