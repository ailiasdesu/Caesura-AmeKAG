#pragma once

#include <cstdint>

namespace Caesura {

// ---------------------------------------------------------------------------
// IAudioFocusService   OS audio-focus / interruption arbitration (Track P5)
// ---------------------------------------------------------------------------
// Minimal, platform-agnostic feed for audio focus state:
//   * Desktop / Web:   no OS arbitration — service stays nothing to do
//   * Android / iOS:   native audio-interruption callbacks post events here
//                      (focus lost / interruption begin -> duck or pause;
//                      gained / end -> resume). Engine listens and drives
//                      IAudioBackend suspend/resume; games/layers can listen
//                      too (e.g. pause gameplay on interruption).

enum class AudioFocusEvent : uint8_t {
    FocusGained = 0,      // app regains exclusive focus (normal playback)
    FocusLost = 1,        // OS denies/removes audio focus
    InterruptionBegin = 2, // transient interruption (call, Siri, alarm...)
    InterruptionEnd = 3,  // transient interruption over
};

enum class AudioFocusState : uint8_t {
    Normal = 0,
    Lost = 1,
    Interrupted = 2,
};

class IAudioFocusListener {
public:
    virtual ~IAudioFocusListener() = default;
    virtual void onAudioFocusEvent(AudioFocusEvent event) = 0;
};

class IAudioFocusService {
public:
    virtual ~IAudioFocusService() = default;

    virtual void addListener(IAudioFocusListener* listener) = 0;
    virtual void removeListener(IAudioFocusListener* listener) = 0;
    virtual void post(AudioFocusEvent event) = 0;
    virtual AudioFocusState currentState() const = 0;
};

} // namespace Caesura
