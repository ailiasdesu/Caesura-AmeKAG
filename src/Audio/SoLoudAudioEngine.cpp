#include "SoLoudAudioEngine.h"
#include <soloud_wav.h>
#include <soloud_wavstream.h>
#include <cstdio>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include "Core/Engine.h"
#include <list>

namespace Caesura {

// -- Internal wave cache ---------------------------------------------------

static std::unordered_map<std::string, std::shared_ptr<SoLoud::AudioSource>> g_waveCache;
static std::list<std::string> g_waveLRU;
static std::unordered_map<std::string, std::list<std::string>::iterator> g_waveLRUMap;

// Detect extension and use WavStream for .ogg/.mp3, Wav for .wav
static bool isStreamFormat(const std::string& file) {
    size_t dot = file.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext = file.substr(dot);
    // case-insensitive compare
    for (auto& c : ext) c = (char)tolower((unsigned char)c);
    return ext == ".ogg" || ext == ".mp3";
}

static std::shared_ptr<SoLoud::AudioSource> loadWave(const std::string& file) {
    auto it = g_waveCache.find(file);
    if (it != g_waveCache.end()) {
        auto mapIt = g_waveLRUMap.find(file);
        if (mapIt != g_waveLRUMap.end()) g_waveLRU.splice(g_waveLRU.begin(), g_waveLRU, mapIt->second);
        return it->second;
    }

    std::shared_ptr<SoLoud::AudioSource> src;

    if (isStreamFormat(file)) {
        auto stream = std::make_shared<SoLoud::WavStream>();
        if (stream->load(file.c_str()) != SoLoud::SO_NO_ERROR) {
            fprintf(stderr, "[Audio] Failed to load stream: %s\n", file.c_str());
            return nullptr;
        }
        src = stream;
    } else {
        auto wav = std::make_shared<SoLoud::Wav>();
        if (wav->load(file.c_str()) != SoLoud::SO_NO_ERROR) {
            fprintf(stderr, "[Audio] Failed to load: %s\n", file.c_str());
            return nullptr;
        }
        src = wav;
    }

    // LRU eviction: remove least recently used when >= 128 entries
    if (g_waveCache.size() >= 128) {
        std::string lruFile = g_waveLRU.back();
        g_waveLRU.pop_back();
        g_waveLRUMap.erase(lruFile);
        g_waveCache.erase(lruFile);
        fprintf(stderr, "[Audio] Wave cache LRU evicted: %s\n", lruFile.c_str());
    }
    g_waveCache[file] = src;
    g_waveLRU.push_front(file);
    g_waveLRUMap[file] = g_waveLRU.begin();
    return src;
}

// -- Lifecycle -------------------------------------------------------------

SoLoudAudioEngine::~SoLoudAudioEngine() {
    shutdown();
}

bool SoLoudAudioEngine::init(){
    CAESURA_ASSERT_MAIN_THREAD();
    if (m_initialized) return true;

    SoLoud::result res = m_soloud.init(
        SoLoud::Soloud::CLIP_ROUNDOFF,
        SoLoud::Soloud::AUTO,
        SoLoud::Soloud::AUTO,
        2
    );
    if (res != SoLoud::SO_NO_ERROR) {
        fprintf(stderr, "[Audio] SoLoud init failed: %d\n", res);
        return false;
    }

    m_soloud.setGlobalVolume(m_globalVolume);

    // Create and play audio buses — all must succeed or init fails
    m_bgmBus.setVolume(1.0f);
    m_bgmBusHandle = m_soloud.play(m_bgmBus);
    if (!m_soloud.isValidVoiceHandle(m_bgmBusHandle)) {
        fprintf(stderr, "[Audio] BGM bus play() returned invalid handle 0\n");
        m_soloud.deinit();
        return false;
    }

    m_voiceBus.setVolume(1.0f);
    m_voiceBusHandle = m_soloud.play(m_voiceBus);
    if (!m_soloud.isValidVoiceHandle(m_voiceBusHandle)) {
        fprintf(stderr, "[Audio] VOICE bus play() returned invalid handle 0\n");
        m_soloud.deinit();
        return false;
    }

    m_seBus.setVolume(1.0f);
    m_seBusHandle = m_soloud.play(m_seBus);
    if (!m_soloud.isValidVoiceHandle(m_seBusHandle)) {
        fprintf(stderr, "[Audio] SE bus play() returned invalid handle 0\n");
        m_soloud.deinit();
        return false;
    }

    m_initialized = true;
    printf("[Audio] SoLoud initialized: 3 buses (BGM, VOICE, SE) ready.\n");
    return true;
}

void SoLoudAudioEngine::shutdown(){
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_initialized) return;
    m_soloud.stopAll();
    m_soloud.deinit();
    g_waveCache.clear();
    g_waveLRU.clear();
    g_waveLRUMap.clear();
    m_activeSE.clear();
    m_initialized = false;
    m_currentBGM = 0;
    m_currentVoice = 0;
    printf("[Audio] SoLoud shut down.\n");
}

void SoLoudAudioEngine::update(float /*deltaTime*/){
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_initialized) return;
    m_soloud.update3dAudio();

    // Cull finished SE handles
    m_activeSE.erase(
        std::remove_if(m_activeSE.begin(), m_activeSE.end(),
            [this](SoLoud::handle h) { return !m_soloud.isValidVoiceHandle(h); }),
        m_activeSE.end());
}

// -- Global volume ---------------------------------------------------------

void SoLoudAudioEngine::setGlobalVolume(float volume){
    CAESURA_ASSERT_MAIN_THREAD();
    m_globalVolume = volume;
    if (m_initialized) m_soloud.setGlobalVolume(volume);
}

float SoLoudAudioEngine::getGlobalVolume() const {
    return m_globalVolume;
}

// -- Per-bus volume --------------------------------------------------------

void SoLoudAudioEngine::setBusVolume(const char* bus, float volume){
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_initialized) return;
    std::string b(bus);
    if (b == "bgm") {
        m_bgmVolume = volume;
        m_bgmBus.setVolume(volume);
    } else if (b == "voice") {
        m_voiceVolume = volume;
        m_voiceBus.setVolume(volume);
    } else if (b == "se") {
        m_seVolume = volume;
        m_seBus.setVolume(volume);
    }
    printf("[Audio] Bus %s volume = %.2f\n", bus, volume);
}

float SoLoudAudioEngine::getBusVolume(const char* bus) const {
    std::string b(bus);
    if (b == "bgm")   return m_bgmVolume;
    if (b == "voice") return m_voiceVolume;
    if (b == "se")    return m_seVolume;
    return 1.0f;
}

void SoLoudAudioEngine::flushWaveCache() {
    g_waveCache.clear();
    g_waveLRU.clear();
    g_waveLRUMap.clear();
    printf("[Audio] Wave cache flushed.\n");
}

// -- BGM: with cross-fade support (Spec [3.1][3.2]) ------------------------
// playBGM cross-fades out old BGM over fadeTime and fades in the new one.
// The fade-out uses fadeVolume() + scheduleStop() for a smooth transition.

unsigned int SoLoudAudioEngine::playBGM(const std::string& file, float fadeTime) {
    if (!m_initialized) return 0;

    auto wav = loadWave(file);
    if (!wav) return 0;

    // Cross-fade out the current BGM if playing
    if (m_currentBGM != 0 && m_soloud.isValidVoiceHandle(m_currentBGM)) {
        m_soloud.fadeVolume(m_currentBGM, 0.0f, fadeTime);
        m_soloud.scheduleStop(m_currentBGM, fadeTime);
    }

    // Start new BGM at volume 0, then fade in
    SoLoud::handle h = m_bgmBus.play(*wav, 0.0f);
    if (h == 0) return 0;

    m_soloud.fadeVolume(h, 1.0f, fadeTime);
    m_currentBGM = h;

    printf("[Audio] BGM: %s (handle %u, fade %.1fs)\n", file.c_str(), h, fadeTime);
    return static_cast<unsigned int>(h);
}

void SoLoudAudioEngine::stopBGM(float fadeTime) {
    if (!m_initialized || m_currentBGM == 0) return;
    if (m_soloud.isValidVoiceHandle(m_currentBGM)) {
        m_soloud.fadeVolume(m_currentBGM, 0.0f, fadeTime);
        m_soloud.scheduleStop(m_currentBGM, fadeTime);
    }
    m_currentBGM = 0;
}

// -- VOICE -----------------------------------------------------------------

unsigned int SoLoudAudioEngine::playVoice(const std::string& file){
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_initialized) return 0;

    auto wav = loadWave(file);
    if (!wav) return 0;

    if (m_currentVoice != 0 && m_soloud.isValidVoiceHandle(m_currentVoice)) {
        m_soloud.fadeVolume(m_currentVoice, 0.0f, 0.05f);
        m_soloud.scheduleStop(m_currentVoice, 0.05f);
    }

    SoLoud::handle h = m_voiceBus.play(*wav);
    if (h == 0) return 0;

    m_currentVoice = h;
    printf("[Audio] Voice: %s (handle %u)\n", file.c_str(), h);
    return static_cast<unsigned int>(h);
}

void SoLoudAudioEngine::stopVoice(){
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_initialized || m_currentVoice == 0) return;
    if (m_soloud.isValidVoiceHandle(m_currentVoice)) {
        m_soloud.fadeVolume(m_currentVoice, 0.0f, 0.05f);
        m_soloud.scheduleStop(m_currentVoice, 0.05f);
    }
    m_currentVoice = 0;
}

// -- SE --------------------------------------------------------------------
// SE handles are tracked in m_activeSE for mass-stop via stopSE().
// Cleanup: dead handles are culled on each playSE/playSE3D call.

void SoLoudAudioEngine::stopSE(){
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_initialized) return;
    for (auto h : m_activeSE) {
        if (m_soloud.isValidVoiceHandle(h)) {
            m_soloud.stop(h);
        }
    }
    m_activeSE.clear();
    printf("[Audio] SE: all sound effects stopped.\n");
}

unsigned int SoLoudAudioEngine::playSE(const std::string& file){
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_initialized) return 0;
    auto wav = loadWave(file);
    if (!wav) return 0;

    // Cull dead handles before adding new one
    m_activeSE.erase(
        std::remove_if(m_activeSE.begin(), m_activeSE.end(),
            [this](SoLoud::handle h) { return !m_soloud.isValidVoiceHandle(h); }),
        m_activeSE.end());

    SoLoud::handle h = m_seBus.play(*wav);
    if (h != 0) {
        m_activeSE.push_back(h);
    }
    printf("[Audio] SE: %s (handle %u)\n", file.c_str(), h);
    return static_cast<unsigned int>(h);
}

unsigned int SoLoudAudioEngine::playSE3D(const std::string& file,
                                          float x, float y, float z) {
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_initialized) return 0;
    auto wav = loadWave(file);
    if (!wav) return 0;

    // Cull dead handles
    m_activeSE.erase(
        std::remove_if(m_activeSE.begin(), m_activeSE.end(),
            [this](SoLoud::handle h) { return !m_soloud.isValidVoiceHandle(h); }),
        m_activeSE.end());

    SoLoud::handle h = m_seBus.play3d(*wav, x, y, z);
    if (h != 0) {
        m_activeSE.push_back(h);
    }
    printf("[Audio] SE 3D: %s at (%.1f,%.1f,%.1f) h=%u\n",
           file.c_str(), x, y, z, h);
    return static_cast<unsigned int>(h);
}

// -- 3D Audio --------------------------------------------------------------

void SoLoudAudioEngine::update3dListener(float posX, float posY, float posZ,
                                          float atX, float atY, float atZ,
                                          float upX, float upY, float upZ) {
    if (!m_initialized) return;
    m_soloud.set3dListenerParameters(posX, posY, posZ,
                                     atX, atY, atZ,
                                     upX, upY, upZ);
}

// -- State query -----------------------------------------------------------

bool SoLoudAudioEngine::isVoicePlaying() {
    return m_initialized && m_currentVoice != 0
        && m_soloud.isValidVoiceHandle(m_currentVoice);
}

bool SoLoudAudioEngine::isBGMPlaying() {
    return m_initialized && m_currentBGM != 0
        && m_soloud.isValidVoiceHandle(m_currentBGM);
}

int SoLoudAudioEngine::activeVoiceCount() {
    return m_initialized ? m_soloud.getActiveVoiceCount() : 0;
}

// -- Playback position (Spec [3.3]) -----------------------------------------

float SoLoudAudioEngine::getPosition(const char* bus) {
    if (!m_initialized) return 0.0f;
    std::string b(bus);
    SoLoud::handle h = 0;
    if (b == "voice") h = m_currentVoice;
    else if (b == "bgm")   h = m_currentBGM;
    if (h != 0 && m_soloud.isValidVoiceHandle(h))
        return (float)m_soloud.getStreamPosition(h);
    return 0.0f;
}

float SoLoudAudioEngine::getLength(const char* bus) {
    if (!m_initialized) return 0.0f;
    std::string b(bus);
    SoLoud::handle h = 0;
    if (b == "voice") h = m_currentVoice;
    else if (b == "bgm")   h = m_currentBGM;
    if (h != 0 && m_soloud.isValidVoiceHandle(h))
        return (float)m_soloud.getStreamTime(h);
    return 0.0f;
}

// -- Fade bus volume without stopping (Spec [3.2]) --------------------------

void SoLoudAudioEngine::fadeVolume(const char* bus, float targetVolume, float fadeTime){
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_initialized) return;
    std::string b(bus);
    if (b == "bgm") {
        m_bgmVolume = targetVolume;
        m_soloud.fadeVolume(m_bgmBusHandle, targetVolume, fadeTime);
    } else if (b == "voice") {
        m_voiceVolume = targetVolume;
        m_soloud.fadeVolume(m_voiceBusHandle, targetVolume, fadeTime);
    } else if (b == "se") {
        m_seVolume = targetVolume;
        m_soloud.fadeVolume(m_seBusHandle, targetVolume, fadeTime);
    }
    printf("[Audio] Bus %s fade to %.2f over %.2fs\n", bus, targetVolume, fadeTime);
}


// -- [10.2.27] Per-SE-handle volume control ---------------------------------

void SoLoudAudioEngine::setSEVolume(unsigned int handle, float volume){
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_initialized || handle == 0) return;
    m_soloud.setVolume(handle, volume);
}

float SoLoudAudioEngine::getSEVolume(unsigned int handle) {
    if (!m_initialized || handle == 0) return 0.0f;
    return m_soloud.getVolume(handle);
}

void SoLoudAudioEngine::stopSEHandle(unsigned int handle){
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_initialized || handle == 0) return;
    m_soloud.stop(handle);
    // Remove from active SE tracking
    auto it = std::find(m_activeSE.begin(), m_activeSE.end(), handle);
    if (it != m_activeSE.end()) m_activeSE.erase(it);
}

} // namespace Caesura
