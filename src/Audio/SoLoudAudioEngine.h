#pragma once
struct lua_State;
#include "../Core/IAudioBackend.h"
#include <soloud.h>
#include <soloud_bus.h>
#include <string>
#include <unordered_map>
#include <list>
#include <memory>
#include <vector>

namespace Caesura {

// ---------------------------------------------------------------------------
// SoLoudAudioEngine   Concrete IAudioBackend using SoLoud
// ---------------------------------------------------------------------------
// Three audio buses: BGM, VOICE, SE.
// Managed via BackendRegistry; created by factory at Lua init time.
// Waveform cache uses LRU eviction (spec [10.2.69]) with O(1) touch via
// unordered_map + list.  Cache is a class member, not a global static.

class SoLoudAudioEngine : public IAudioBackend {
public:
    SoLoudAudioEngine() = default;
    void setLuaState(lua_State* L) { m_luaState = L; }
    ~SoLoudAudioEngine() override;

    SoLoudAudioEngine(const SoLoudAudioEngine&) = delete;
    SoLoudAudioEngine& operator=(const SoLoudAudioEngine&) = delete;

    // -- IAudioBackend interface -------------------------------------------
    bool init() override;
    void shutdown() override;
    void update(float deltaTime) override;

    unsigned int playBGM(const std::string& file, float fadeTime = 1.0f) override;
    void stopBGM(float fadeTime = 1.0f) override;

    unsigned int playVoice(const std::string& file) override;
    void stopVoice() override;

    unsigned int playSE(const std::string& file) override;
    unsigned int playSE3D(const std::string& file, float x, float y, float z) override;
    void stopSE() override;
    void setSEVolume(unsigned int handle, float volume) override;
    float getSEVolume(unsigned int handle) override;
    void stopSEHandle(unsigned int handle) override;

    void update3dListener(float posX, float posY, float posZ,
                          float atX, float atY, float atZ,
                          float upX = 0.0f, float upY = 1.0f,
                          float upZ = 0.0f) override;

    void setGlobalVolume(float volume) override;
    float getGlobalVolume() const override;

    void setBusVolume(const char* bus, float volume) override;
    float getBusVolume(const char* bus) const override;

    void flushWaveCache() override;

    bool isVoicePlaying() override;
    bool isBGMPlaying() override;
    bool isSEPlaying() override;
    int activeVoiceCount() override;

    float getPosition(const char* bus) override;
    float getLength(const char* bus) override;
    void fadeVolume(const char* bus, float targetVolume, float fadeTime) override;

    const char* getBackendName() const override { return "SoLoud"; }

    // -- Direct SoLoud access (for advanced use) ---------------------------
    SoLoud::Soloud& soloud() { return m_soloud; }
    SoLoud::Bus& bgmBus()    { return m_bgmBus; }
    SoLoud::Bus& voiceBus()  { return m_voiceBus; }
    SoLoud::Bus& seBus()     { return m_seBus; }

private:
    // -- Internal helpers -------------------------------------------------
    std::shared_ptr<SoLoud::AudioSource> loadWave(const std::string& file);

    SoLoud::Soloud m_soloud;
    lua_State* m_luaState = nullptr;
    SoLoud::Bus    m_bgmBus;
    SoLoud::Bus    m_voiceBus;
    SoLoud::Bus    m_seBus;
    SoLoud::handle m_bgmBusHandle   = 0;
    SoLoud::handle m_voiceBusHandle = 0;
    SoLoud::handle m_seBusHandle    = 0;

    unsigned int m_currentVoice = 0;
    unsigned int m_currentBGM   = 0;

    // SE handles: tracked per-play for mass-stop via stopSE()
    std::vector<SoLoud::handle> m_activeSE;

    // Waveform LRU cache (spec [3.1], [10.2.69]) -- class member, not global static
    std::unordered_map<std::string, std::shared_ptr<SoLoud::AudioSource>> m_waveCache;
    std::list<std::string> m_waveLRU;
    std::unordered_map<std::string, std::list<std::string>::iterator> m_waveLRUMap;

    bool m_initialized = false;
    float m_globalVolume = 1.0f;

    // Per-bus volume state for persistence
    float m_bgmVolume   = 1.0f;
    float m_voiceVolume = 1.0f;
    float m_seVolume    = 1.0f;
};

} // namespace Caesura



