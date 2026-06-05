#pragma once
#include <string>

namespace Caesura {

// ---------------------------------------------------------------------------
// IAudioBackend   Abstract audio backend interface
// ---------------------------------------------------------------------------
// Concrete implementations: SoLoudAudioEngine, (future) OpenAL, FMOD, etc.
// All Lua-side audio calls dispatch through this interface.
// Backend instances are managed by BackendRegistry and accessed via handles.

class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    // -- Lifecycle ---------------------------------------------------------
    virtual bool init() = 0;
    virtual void shutdown() = 0;
    virtual void update(float deltaTime) = 0;

    // -- BGM bus: background music with cross-fade support -----------------
    virtual unsigned int playBGM(const std::string& file, float fadeTime = 1.0f) = 0;
    virtual void stopBGM(float fadeTime = 1.0f) = 0;

    // -- VOICE bus: voice lines with absolute interrupt --------------------
    virtual unsigned int playVoice(const std::string& file) = 0;
    virtual void stopVoice() = 0;

    // -- SE bus: sound effects (2D and 3D spatial) ------------------------
    virtual unsigned int playSE(const std::string& file) = 0;
    virtual unsigned int playSE3D(const std::string& file,
                                   float x, float y, float z) = 0;
    virtual void stopSE() = 0;

    // -- 3D Audio ----------------------------------------------------------
    virtual void update3dListener(float posX, float posY, float posZ,
                                   float atX, float atY, float atZ,
                                   float upX = 0.0f, float upY = 1.0f,
                                   float upZ = 0.0f) = 0;

    // -- Global volume -----------------------------------------------------
    virtual void setGlobalVolume(float volume) = 0;
    virtual float getGlobalVolume() const = 0;

    // -- Per-bus volume ----------------------------------------------------
    virtual void setBusVolume(const char* bus, float volume) = 0;
    virtual float getBusVolume(const char* bus) const = 0;

    // -- Wave cache ---------------------------------------------------------
    virtual void flushWaveCache() = 0;

    // -- State query -------------------------------------------------------
    virtual bool isVoicePlaying() = 0;
    virtual bool isBGMPlaying() = 0;
    virtual int activeVoiceCount() = 0;

    // -- Playback position / length / fade (Spec [3.2][3.3]) -----------
    virtual float getPosition(const char* bus) = 0;
    virtual float getLength(const char* bus) = 0;
    virtual void fadeVolume(const char* bus, float targetVolume, float fadeTime) = 0;


    // -- Backend identification --------------------------------------------
    virtual const char* getBackendName() const = 0;
};

} // namespace Caesura