#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace Caesura {

// Story-owned playback. An empty path means silence. User master/bus volumes
// belong to device preferences and are deliberately outside the slot state.
struct AudioRestoreState {
    std::string bgmPath;
    double position = 0.0;
    float gain = 1.0f;
    bool looping = false;
    // Optional precise source-domain cursor. sourceRate==0 denotes a legacy
    // seconds-only snapshot; fraction is a 20-bit engine sample phase.
    uint64_t framePosition = 0;
    uint32_t frameFraction = 0;
    uint32_t sourceRate = 0;
    uint32_t outputRate = 0;
};

class IPreparedAudioState {
public:
    virtual ~IPreparedAudioState() = default;
    virtual const AudioRestoreState& description() const = 0;
};

class IAudioRestore {
public:
    virtual ~IAudioRestore() = default;
    virtual AudioRestoreState captureAudioState() = 0;
    // Decode/open an independent copy of the supplied encoded asset, without
    // touching active voices, caches, quotas or user volume preferences.
    virtual std::unique_ptr<IPreparedAudioState> prepareAudioState(
        const AudioRestoreState& state, const uint8_t* bytes, size_t size) = 0;
    // Consumes the preparation. BGM resumes at its position; voice/SE stop.
    // Any failure after commit leaves this session silent.
    virtual bool applyAudioState(std::unique_ptr<IPreparedAudioState> prepared) = 0;
    virtual void stopSessionAudio() = 0;
};

} // namespace Caesura
