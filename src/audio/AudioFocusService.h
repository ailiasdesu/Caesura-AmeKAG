#pragma once

#include "api/IAudioFocusService.h"

#include <algorithm>
#include <mutex>
#include <vector>

namespace Caesura {

// Default focus hub: state machine + ordered listener dispatch on the
// posting thread (same contract as LifecycleService). Never touches audio
// itself — consumers decide what to pause.
class AudioFocusService final : public IAudioFocusService {
public:
    AudioFocusService() = default;

    void addListener(IAudioFocusListener* listener) override {
        if (!listener) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        if (std::find(m_listeners.begin(), m_listeners.end(), listener) != m_listeners.end()) {
            return;
        }
        m_listeners.push_back(listener);
    }

    void removeListener(IAudioFocusListener* listener) override {
        if (!listener) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_listeners.erase(
            std::remove(m_listeners.begin(), m_listeners.end(), listener),
            m_listeners.end());
    }

    void post(AudioFocusEvent event) override {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            transitionLocked(event);
        }
        std::vector<IAudioFocusListener*> snapshot;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            snapshot = m_listeners;
        }
        for (IAudioFocusListener* l : snapshot) {
            if (l) l->onAudioFocusEvent(event);
        }
    }

    AudioFocusState currentState() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_state;
    }

private:
    void transitionLocked(AudioFocusEvent event) {
        switch (event) {
            case AudioFocusEvent::FocusLost:      m_state = AudioFocusState::Lost; break;
            case AudioFocusEvent::FocusGained:    m_state = AudioFocusState::Normal; break;
            case AudioFocusEvent::InterruptionBegin: m_state = AudioFocusState::Interrupted; break;
            case AudioFocusEvent::InterruptionEnd:
                if (m_state == AudioFocusState::Interrupted) m_state = AudioFocusState::Normal;
                break;
        }
    }

    mutable std::mutex m_mutex;
    AudioFocusState m_state = AudioFocusState::Normal;
    std::vector<IAudioFocusListener*> m_listeners;
};

} // namespace Caesura
