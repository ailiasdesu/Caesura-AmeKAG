#include "SoLoudAudioEngine.h"
#include "di/api/ThreadAssert.h"
#include "di/BackendRegistry.h"
#include <soloud_wav.h>
#include <soloud_wavstream.h>
#include <cstdio>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <list>
#include <limits>
namespace Caesura {


// Detect extension and use WavStream for .ogg/.mp3, Wav for .wav
static bool isStreamFormat(const std::string& file) {
    size_t dot = file.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext = file.substr(dot);
    // case-insensitive compare
    for (auto& c : ext) c = (char)tolower((unsigned char)c);
    return ext == ".ogg" || ext == ".mp3" || ext == ".flac";
}

std::shared_ptr<SoLoud::AudioSource> SoLoudAudioEngine::loadWave(const std::string& file) {
    auto it = m_waveCache.find(file);
    if (it != m_waveCache.end()) {
        auto mapIt = m_waveLRUMap.find(file);
        if (mapIt != m_waveLRUMap.end()) m_waveLRU.splice(m_waveLRU.begin(), m_waveLRU, mapIt->second);
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
    if (m_waveCache.size() >= 128) {
        std::string lruFile = m_waveLRU.back();
        m_waveLRU.pop_back();
        m_waveLRUMap.erase(lruFile);
        m_waveCache.erase(lruFile);
        fprintf(stderr, "[Audio] Wave cache LRU evicted: %s\n", lruFile.c_str());
    }
    m_waveCache[file] = src;
    m_waveLRU.push_front(file);
    m_waveLRUMap[file] = m_waveLRU.begin();
    return src;
}

// -- Lifecycle -------------------------------------------------------------

SoLoudAudioEngine::~SoLoudAudioEngine() {
    shutdown();
}

bool SoLoudAudioEngine::init(){
    CAESURA_ASSERT_MAIN_THREAD();
    if (m_initialized) return true;
    m_voiceCompletionsPending = 0;

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

    // Create and play audio buses -- all must succeed or init fails
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

    const std::size_t allocatedHandles = m_activeSE.size()
        + m_retiringBGM.size()
        + m_retiringVoice.size()
        + (m_currentBGM != 0 ? 1u : 0u)
        + (m_currentVoice != 0 ? 1u : 0u);
    m_soloud.stopAll();
    m_soloud.deinit();
    m_waveCache.clear();
    m_waveLRU.clear();
    m_waveLRUMap.clear();
    m_activeSE.clear();
    m_retiringBGM.clear();
    m_retiringVoice.clear();
    m_initialized = false;
    m_currentBGM = 0;
    m_currentVoice = 0;
    m_voiceCompletionsPending = 0;
    m_bgmBusHandle = 0;
    m_voiceBusHandle = 0;
    m_seBusHandle = 0;
    releaseAudioHandles(allocatedHandles);
    printf("[Audio] SoLoud shut down.\n");
}

void SoLoudAudioEngine::update(float /*deltaTime*/){
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_initialized) return;
    m_soloud.update3dAudio();
    cullFinishedHandles();
}

void SoLoudAudioEngine::releaseAudioHandles(std::size_t count) {
    auto& registry = BackendRegistry::instance();
    for (std::size_t i = 0; i < count; ++i) {
        registry.release("audio_handles");
    }
}

void SoLoudAudioEngine::retireHandle(
    SoLoud::handle handle,
    float fadeTime,
    std::vector<SoLoud::handle>& retiringHandles) {
    if (handle == 0) return;

    if (!m_soloud.isValidVoiceHandle(handle)) {
        releaseAudioHandles(1);
        return;
    }

    if (fadeTime <= 0.0f) {
        m_soloud.stop(handle);
        releaseAudioHandles(1);
        return;
    }

    m_soloud.fadeVolume(handle, 0.0f, fadeTime);
    m_soloud.scheduleStop(handle, fadeTime);
    retiringHandles.push_back(handle);
}

void SoLoudAudioEngine::stopRetiringHandles(
    std::vector<SoLoud::handle>& retiringHandles) {
    const std::size_t handleCount = retiringHandles.size();
    for (SoLoud::handle handle : retiringHandles) {
        if (m_soloud.isValidVoiceHandle(handle)) {
            m_soloud.stop(handle);
        }
    }
    retiringHandles.clear();
    releaseAudioHandles(handleCount);
}

void SoLoudAudioEngine::cullFinishedHandles() {
    std::size_t released = 0;

    if (m_currentBGM != 0 && !m_soloud.isValidVoiceHandle(m_currentBGM)) {
        m_currentBGM = 0;
        ++released;
    }
    if (m_currentVoice != 0 && !m_soloud.isValidVoiceHandle(m_currentVoice)) {
        m_currentVoice = 0;
        if (m_voiceCompletionsPending <
            std::numeric_limits<unsigned int>::max()) {
            ++m_voiceCompletionsPending;
        }
        ++released;
    }

    const auto firstFinished = std::remove_if(
        m_activeSE.begin(), m_activeSE.end(),
        [this, &released](SoLoud::handle handle) {
            if (m_soloud.isValidVoiceHandle(handle)) return false;
            ++released;
            return true;
        });
    m_activeSE.erase(firstFinished, m_activeSE.end());

    const auto cullRetiring = [this, &released](auto& retiringHandles) {
        const auto firstInvalid = std::remove_if(
            retiringHandles.begin(), retiringHandles.end(),
            [this, &released](SoLoud::handle handle) {
                if (m_soloud.isValidVoiceHandle(handle)) return false;
                ++released;
                return true;
            });
        retiringHandles.erase(firstInvalid, retiringHandles.end());
    };
    cullRetiring(m_retiringBGM);
    cullRetiring(m_retiringVoice);
    releaseAudioHandles(released);
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
    m_waveCache.clear();
    m_waveLRU.clear();
    m_waveLRUMap.clear();
    printf("[Audio] Wave cache flushed.\n");
}

// -- BGM: with cross-fade support (Spec [3.1][3.2]) ------------------------
// playBGM cross-fades out old BGM over fadeTime and fades in the new one.
// The fade-out uses fadeVolume() + scheduleStop() for a smooth transition.

unsigned int SoLoudAudioEngine::playBGM(const std::string& file, float fadeTime) {
    if (!m_initialized) return 0;

    auto wav = loadWave(file);
    if (!wav) return 0;

    cullFinishedHandles();
    auto& registry = BackendRegistry::instance();
    if (!registry.tryAlloc("audio_handles")) return 0;

    // Start new BGM at volume 0, then fade in
    SoLoud::handle h = m_bgmBus.play(*wav, 0.0f);
    if (h == 0 || !m_soloud.isValidVoiceHandle(h)) {
        registry.release("audio_handles");
        return 0;
    }

    // Retire the previous BGM only after replacement creation succeeds. Its
    // quota remains held until the physical SoLoud voice actually stops.
    if (m_currentBGM != 0) {
        retireHandle(m_currentBGM, fadeTime, m_retiringBGM);
    }

    m_soloud.fadeVolume(h, 1.0f, fadeTime);
    m_currentBGM = h;

    printf("[Audio] BGM: %s (handle %u, fade %.1fs)\n", file.c_str(), h, fadeTime);
    return static_cast<unsigned int>(h);
}

void SoLoudAudioEngine::stopBGM(float fadeTime) {
    if (!m_initialized) return;
    stopRetiringHandles(m_retiringBGM);
    if (m_currentBGM == 0) return;

    const SoLoud::handle current = m_currentBGM;
    m_currentBGM = 0;
    retireHandle(current, fadeTime, m_retiringBGM);
}

// -- VOICE -----------------------------------------------------------------

unsigned int SoLoudAudioEngine::playVoice(const std::string& file){
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_initialized) return 0;

    auto wav = loadWave(file);
    if (!wav) return 0;

    cullFinishedHandles();
    auto& registry = BackendRegistry::instance();
    if (!registry.tryAlloc("audio_handles")) return 0;

    SoLoud::handle h = m_voiceBus.play(*wav);
    if (h == 0 || !m_soloud.isValidVoiceHandle(h)) {
        registry.release("audio_handles");
        return 0;
    }

    if (m_currentVoice != 0) {
        retireHandle(m_currentVoice, 0.05f, m_retiringVoice);
    }

    m_currentVoice = h;
    printf("[Audio] Voice: %s (handle %u)\n", file.c_str(), h);
    return static_cast<unsigned int>(h);
}

void SoLoudAudioEngine::stopVoice(){
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_initialized) return;
    stopRetiringHandles(m_retiringVoice);
    if (m_currentVoice == 0) return;

    const SoLoud::handle current = m_currentVoice;
    m_currentVoice = 0;
    retireHandle(current, 0.05f, m_retiringVoice);
}

// -- SE --------------------------------------------------------------------
// SE handles are tracked in m_activeSE for mass-stop via stopSE().
// Dead handles are culled during update, state queries, and before playback.

void SoLoudAudioEngine::stopSE(){
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_initialized) return;
    const std::size_t handleCount = m_activeSE.size();
    for (auto h : m_activeSE) {
        if (m_soloud.isValidVoiceHandle(h)) {
            m_soloud.stop(h);
        }
    }
    m_activeSE.clear();
    printf("[Audio] SE: all sound effects stopped.\n");
    releaseAudioHandles(handleCount);
}

unsigned int SoLoudAudioEngine::playSE(const std::string& file){
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_initialized) return 0;
    auto wav = loadWave(file);
    if (!wav) return 0;

    cullFinishedHandles();
    auto& registry = BackendRegistry::instance();
    if (!registry.tryAlloc("audio_handles")) return 0;

    SoLoud::handle h = m_seBus.play(*wav);
    if (h == 0 || !m_soloud.isValidVoiceHandle(h)) {
        registry.release("audio_handles");
        return 0;
    }
    m_activeSE.push_back(h);
    printf("[Audio] SE: %s (handle %u)\n", file.c_str(), h);
    return static_cast<unsigned int>(h);
}

unsigned int SoLoudAudioEngine::playSE3D(const std::string& file,
                                          float x, float y, float z) {
    CAESURA_ASSERT_MAIN_THREAD();
    if (!m_initialized) return 0;
    auto wav = loadWave(file);
    if (!wav) return 0;

    cullFinishedHandles();
    auto& registry = BackendRegistry::instance();
    if (!registry.tryAlloc("audio_handles")) return 0;

    SoLoud::handle h = m_seBus.play3d(*wav, x, y, z);
    if (h == 0 || !m_soloud.isValidVoiceHandle(h)) {
        registry.release("audio_handles");
        return 0;
    }
    m_activeSE.push_back(h);
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

unsigned int SoLoudAudioEngine::consumeVoiceCompletions() {
    const unsigned int completed = m_voiceCompletionsPending;
    m_voiceCompletionsPending = 0;
    return completed;
}

bool SoLoudAudioEngine::isVoicePlaying() {
    if (!m_initialized) return false;
    cullFinishedHandles();
    return m_currentVoice != 0;
}

bool SoLoudAudioEngine::isBGMPlaying() {
    if (!m_initialized) return false;
    cullFinishedHandles();
    return m_currentBGM != 0;
}

bool SoLoudAudioEngine::isSEPlaying() {
    if (!m_initialized) return false;
    cullFinishedHandles();
    return !m_activeSE.empty();
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
    auto it = std::find(m_activeSE.begin(), m_activeSE.end(), handle);
    if (it == m_activeSE.end()) return;

    if (m_soloud.isValidVoiceHandle(handle)) {
        m_soloud.stop(handle);
    }
    m_activeSE.erase(it);
    BackendRegistry::instance().release("audio_handles");
}

} // namespace Caesura
