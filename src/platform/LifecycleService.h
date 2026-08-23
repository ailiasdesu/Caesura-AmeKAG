#pragma once

#include "api/ILifecycleService.h"

#include <algorithm>
#include <mutex>
#include <vector>

namespace Caesura {

// Default hub: registered listeners dispatched in registration order on the
// posting thread. Thread-safe registration; dispatch copies the listener set
// under the lock (a listener that removes itself during dispatch is safe).
class LifecycleService final : public ILifecycleService {
public:
    void addListener(ILifecycleListener* listener) override {
        if (!listener) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        if (std::find(m_listeners.begin(), m_listeners.end(), listener) != m_listeners.end()) {
            return; // duplicates ignored
        }
        m_listeners.push_back(listener);
    }

    void removeListener(ILifecycleListener* listener) override {
        if (!listener) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_listeners.erase(
            std::remove(m_listeners.begin(), m_listeners.end(), listener),
            m_listeners.end());
    }

    void post(LifecycleEvent event) override {
        std::vector<ILifecycleListener*> snapshot;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            snapshot = m_listeners;
        }
        for (ILifecycleListener* l : snapshot) {
            if (l) l->onLifecycleEvent(event);
        }
    }

private:
    std::mutex m_mutex;
    std::vector<ILifecycleListener*> m_listeners;
};

} // namespace Caesura
