#include "doctest.h"
#include <algorithm>
#include <cstring>
#include "audio/SoLoudAudioEngine.h"
#include "di/BackendRegistry.h"
#include "di/api/ISandboxQuota.h"

using namespace Caesura;

namespace {

class AudioQuota final : public ISandboxQuota {
public:
    explicit AudioQuota(int limit) : m_limit(limit) {}

    void setLuaState(lua_State*) override {}

    bool tryAlloc(const char* kind) override {
        ++tryCalls;
        if (std::strcmp(kind, "audio_handles") != 0) {
            ++unexpectedKinds;
            return false;
        }
        if (activeCount >= m_limit) return false;
        ++activeCount;
        peakCount = std::max(peakCount, activeCount);
        return true;
    }

    void release(const char* kind) override {
        ++releaseCalls;
        if (std::strcmp(kind, "audio_handles") != 0) {
            ++unexpectedKinds;
            return;
        }
        if (activeCount == 0) {
            ++releaseUnderflows;
            return;
        }
        --activeCount;
    }

    int count(const char*) override { return activeCount; }
    int maxLimit(const char*) override { return m_limit; }

    int activeCount = 0;
    int peakCount = 0;
    int tryCalls = 0;
    int releaseCalls = 0;
    int releaseUnderflows = 0;
    int unexpectedKinds = 0;

private:
    int m_limit;
};

class ScopedAudioQuota final {
public:
    explicit ScopedAudioQuota(ISandboxQuota& quota)
        : m_registry(BackendRegistry::instance()),
          m_previous(m_registry.getSandboxQuota()) {
        m_registry.setSandboxQuota(&quota);
    }

    ~ScopedAudioQuota() { m_registry.setSandboxQuota(m_previous); }

    ScopedAudioQuota(const ScopedAudioQuota&) = delete;
    ScopedAudioQuota& operator=(const ScopedAudioQuota&) = delete;

private:
    BackendRegistry& m_registry;
    ISandboxQuota* m_previous;
};

} // namespace

TEST_CASE("SoLoudAudioEngine::name") {
    SoLoudAudioEngine eng;
    CHECK(strcmp(eng.getBackendName(), "SoLoud") == 0);
}

TEST_CASE("SoLoudAudioEngine::init succeeds") {
    SoLoudAudioEngine eng;
    CHECK(eng.init());
    CHECK(eng.isBGMPlaying() == false);
    CHECK(eng.isVoicePlaying() == false);
    CHECK(eng.activeVoiceCount() >= 0);
}

TEST_CASE("SoLoudAudioEngine::global volume") {
    SoLoudAudioEngine eng;
    eng.init();
    eng.setGlobalVolume(0.5f);
    CHECK(eng.getGlobalVolume() == doctest::Approx(0.5f));
    eng.setGlobalVolume(1.0f);
    CHECK(eng.getGlobalVolume() == doctest::Approx(1.0f));
}

TEST_CASE("SoLoudAudioEngine::bus volume persistence") {
    SoLoudAudioEngine eng;
    eng.init();
    eng.setBusVolume("bgm", 0.8f);
    CHECK(eng.getBusVolume("bgm") == doctest::Approx(0.8f));
    eng.setBusVolume("voice", 0.6f);
    CHECK(eng.getBusVolume("voice") == doctest::Approx(0.6f));
}

TEST_CASE("SoLoudAudioEngine::fade volume does not crash") {
    SoLoudAudioEngine eng;
    eng.init();
    eng.fadeVolume("bgm", 0.0f, 0.5f);
    eng.fadeVolume("voice", 0.5f, 1.0f);
    eng.fadeVolume("se", 1.0f, 0.3f);
}

TEST_CASE("SoLoudAudioEngine::shutdown idempotent") {
    SoLoudAudioEngine eng;
    eng.init();
    eng.shutdown();
    eng.shutdown();
    CHECK(eng.activeVoiceCount() == 0);
}

TEST_CASE("SoLoudAudioEngine::playSE returns handle") {
    SoLoudAudioEngine eng;
    eng.init();
    // Play non-existent file returns 0, doesn't crash
    unsigned int h = eng.playSE("nonexistent.wav");
    CHECK(h == 0);
}

TEST_CASE("SoLoudAudioEngine::LRU cache survives multiple plays") {
    SoLoudAudioEngine eng;
    eng.init();
    for (int i = 0; i < 10; i++) {
        eng.playSE("nonexistent.wav");  // each call attempts load
    }
    // Cache operations should not crash
}


TEST_CASE("SoLoudAudioEngine::load WAV format") {
    SoLoudAudioEngine eng;
    eng.init();
    unsigned int h = eng.playSE("tests/audio/silence.wav");
    CHECK(h > 0);
    eng.stopSE();
}

TEST_CASE("SoLoudAudioEngine::load FLAC format") {
    SoLoudAudioEngine eng;
    eng.init();
    unsigned int h = eng.playSE("tests/audio/silence.flac");
    CHECK(h > 0);
    eng.stopSE();
}

TEST_CASE("SoLoudAudioEngine::unsupported format returns 0 no crash") {
    SoLoudAudioEngine eng;
    eng.init();
    unsigned int h = eng.playSE("CMakeLists.txt");
    CHECK(h == 0);
    unsigned int h2 = eng.playSE("");
    CHECK(h2 == 0);
}

// =============================================================================
// Expanded: BGM, Voice, SE3D, SE control, 3D, position, flush
// =============================================================================

TEST_CASE("SoLoudAudioEngine::playBGM and stopBGM with silence") {
    SoLoudAudioEngine eng;
    eng.init();
    unsigned int h = eng.playBGM("tests/audio/silence.wav", 0.0f);
    CHECK(h > 0);
    CHECK(eng.isBGMPlaying());
    eng.stopBGM(0.0f);
}

TEST_CASE("SoLoudAudioEngine::playVoice and stopVoice with silence") {
    SoLoudAudioEngine eng;
    eng.init();
    unsigned int h = eng.playVoice("tests/audio/silence.wav");
    CHECK(h > 0);
    CHECK(eng.isVoicePlaying());
    eng.stopVoice();
    CHECK(eng.consumeVoiceCompletions() == 0);
}

TEST_CASE("SoLoudAudioEngine reports each naturally finished current voice once") {
    SoLoudAudioEngine eng;
    REQUIRE(eng.init());
    const unsigned int handle = eng.playVoice("tests/audio/silence.wav");
    REQUIRE(handle != 0);
    eng.soloud().setLooping(handle, true);
    CHECK(eng.consumeVoiceCompletions() == 0);

    eng.soloud().stop(handle);
    REQUIRE_FALSE(eng.soloud().isValidVoiceHandle(handle));
    eng.update(0.0f);

    CHECK(eng.consumeVoiceCompletions() == 1);
    CHECK(eng.consumeVoiceCompletions() == 0);
}

TEST_CASE("SoLoudAudioEngine::playSE3D with silence") {
    SoLoudAudioEngine eng;
    eng.init();
    unsigned int h = eng.playSE3D("tests/audio/silence.wav", 0, 0, -5);
    CHECK(h > 0);
    eng.stopSE();
}

TEST_CASE("SoLoudAudioEngine::setSEVolume and stopSEHandle") {
    SoLoudAudioEngine eng;
    eng.init();
    unsigned int h = eng.playSE("tests/audio/silence.wav");
    REQUIRE(h > 0);
    eng.setSEVolume(h, 0.5f);
    CHECK(eng.getSEVolume(h) == doctest::Approx(0.5f));
    eng.stopSEHandle(h);
    // stopSE handle 0 should not crash
    eng.stopSEHandle(0);
}

TEST_CASE("SoLoudAudioEngine::update3dListener does not crash") {
    SoLoudAudioEngine eng;
    eng.init();
    eng.update3dListener(0, 0, 0, 1, 0, 0);
    eng.update3dListener(10, 5, -3, 0, 1, 0, 0, 1, 0);
}

TEST_CASE("SoLoudAudioEngine::isSEPlaying returns false initially") {
    SoLoudAudioEngine eng;
    eng.init();
    CHECK_FALSE(eng.isSEPlaying());
}

TEST_CASE("SoLoudAudioEngine::getPosition and getLength return zero initially") {
    SoLoudAudioEngine eng;
    eng.init();
    CHECK(eng.getPosition("bgm") == 0.0f);
    CHECK(eng.getLength("bgm") == 0.0f);
    CHECK(eng.getPosition("voice") == 0.0f);
    CHECK(eng.getPosition("se") == 0.0f);
}

TEST_CASE("SoLoudAudioEngine::update does not crash") {
    SoLoudAudioEngine eng;
    eng.init();
    eng.update(0.016f);
    eng.update(0.0f);
}

TEST_CASE("SoLoudAudioEngine::flushWaveCache does not crash") {
    SoLoudAudioEngine eng;
    eng.init();
    eng.playSE("tests/audio/silence.wav");
    eng.flushWaveCache();
    eng.flushWaveCache();  // idempotent
}

TEST_CASE("SoLoudAudioEngine rejects playback when audio handle quota is exhausted") {
    AudioQuota quota(0);
    ScopedAudioQuota scopedQuota(quota);
    SoLoudAudioEngine eng;
    REQUIRE(eng.init());
    const int baselineVoices = eng.activeVoiceCount();

    CHECK(eng.playBGM("tests/audio/silence.wav", 0.0f) == 0);
    CHECK(eng.playVoice("tests/audio/silence.wav") == 0);
    CHECK(eng.playSE("tests/audio/silence.wav") == 0);
    CHECK(eng.playSE3D("tests/audio/silence.wav", 0.0f, 0.0f, 0.0f) == 0);

    CHECK(quota.tryCalls == 4);
    CHECK(quota.activeCount == 0);
    CHECK(quota.releaseCalls == 0);
    CHECK(quota.unexpectedKinds == 0);
    CHECK(eng.activeVoiceCount() == baselineVoices);
}

TEST_CASE("SoLoudAudioEngine releases reserved quota when SoLoud creation fails") {
    AudioQuota quota(1);
    ScopedAudioQuota scopedQuota(quota);
    SoLoudAudioEngine eng;
    REQUIRE(eng.init());

    SoLoud::BusInstance* instance = eng.seBus().mInstance;
    REQUIRE(instance != nullptr);
    eng.seBus().mInstance = nullptr;
    CHECK(eng.playSE("tests/audio/silence.wav") == 0);
    eng.seBus().mInstance = instance;

    CHECK(quota.tryCalls == 1);
    CHECK(quota.releaseCalls == 1);
    CHECK(quota.activeCount == 0);
    CHECK(quota.releaseUnderflows == 0);
}

TEST_CASE("SoLoudAudioEngine keeps current BGM when replacement quota is denied") {
    AudioQuota quota(1);
    ScopedAudioQuota scopedQuota(quota);
    SoLoudAudioEngine eng;
    REQUIRE(eng.init());

    const unsigned int current = eng.playBGM("tests/audio/silence.wav", 0.0f);
    REQUIRE(current != 0);
    eng.soloud().setLooping(current, true);
    const int activeVoices = eng.activeVoiceCount();

    CHECK(eng.playBGM("tests/audio/silence.wav", 0.0f) == 0);
    CHECK(eng.isBGMPlaying());
    CHECK(eng.activeVoiceCount() == activeVoices);
    CHECK(quota.activeCount == 1);
    CHECK(quota.tryCalls == 2);
    CHECK(quota.releaseCalls == 0);

    eng.stopBGM(0.0f);
    CHECK(quota.activeCount == 0);
    CHECK(quota.releaseCalls == 1);
}

TEST_CASE("SoLoudAudioEngine stopSE releases every tracked handle exactly once") {
    AudioQuota quota(8);
    ScopedAudioQuota scopedQuota(quota);
    SoLoudAudioEngine eng;
    REQUIRE(eng.init());

    REQUIRE(eng.playSE("tests/audio/silence.wav") != 0);
    REQUIRE(eng.playSE3D("tests/audio/silence.wav", 0.0f, 0.0f, -1.0f) != 0);
    REQUIRE(quota.activeCount == 2);

    eng.stopSE();
    CHECK(quota.activeCount == 0);
    CHECK(quota.releaseCalls == 2);
    CHECK(quota.releaseUnderflows == 0);

    eng.stopSE();
    CHECK(quota.releaseCalls == 2);
}

TEST_CASE("SoLoudAudioEngine stopSEHandle releases only a tracked handle") {
    AudioQuota quota(8);
    ScopedAudioQuota scopedQuota(quota);
    SoLoudAudioEngine eng;
    REQUIRE(eng.init());

    const unsigned int handle = eng.playSE("tests/audio/silence.wav");
    REQUIRE(handle != 0);
    REQUIRE(quota.activeCount == 1);

    eng.stopSEHandle(handle);
    CHECK(quota.activeCount == 0);
    CHECK(quota.releaseCalls == 1);

    eng.stopSEHandle(handle);
    eng.stopSEHandle(0);
    CHECK(quota.releaseCalls == 1);
    CHECK(quota.releaseUnderflows == 0);
}

TEST_CASE("SoLoudAudioEngine update releases naturally finished SE handles") {
    AudioQuota quota(8);
    ScopedAudioQuota scopedQuota(quota);
    SoLoudAudioEngine eng;
    REQUIRE(eng.init());

    const unsigned int handle = eng.playSE("tests/audio/silence.wav");
    REQUIRE(handle != 0);
    REQUIRE(quota.activeCount == 1);

    eng.soloud().stop(handle);
    REQUIRE_FALSE(eng.soloud().isValidVoiceHandle(handle));
    eng.update(0.0f);

    CHECK(quota.activeCount == 0);
    CHECK(quota.releaseCalls == 1);
    CHECK_FALSE(eng.isSEPlaying());
}

TEST_CASE("SoLoudAudioEngine BGM and voice replacement keep quota counts symmetric") {
    AudioQuota quota(8);
    ScopedAudioQuota scopedQuota(quota);
    SoLoudAudioEngine eng;
    REQUIRE(eng.init());

    REQUIRE(eng.playBGM("tests/audio/silence.wav", 0.0f) != 0);
    REQUIRE(quota.activeCount == 1);
    REQUIRE(eng.playBGM("tests/audio/silence.wav", 0.0f) != 0);
    CHECK(quota.activeCount == 1);
    CHECK(quota.releaseCalls == 1);
    CHECK(quota.peakCount == 2);
    eng.stopBGM(0.0f);
    CHECK(quota.activeCount == 0);
    CHECK(quota.releaseCalls == 2);

    const unsigned int firstVoice = eng.playVoice("tests/audio/silence.wav");
    REQUIRE(firstVoice != 0);
    eng.soloud().setLooping(firstVoice, true);
    REQUIRE(quota.activeCount == 1);
    const unsigned int secondVoice = eng.playVoice("tests/audio/silence.wav");
    REQUIRE(secondVoice != 0);
    eng.soloud().setLooping(secondVoice, true);
    CHECK(quota.activeCount == 2);
    CHECK(quota.releaseCalls == 2);

    eng.soloud().stop(firstVoice);
    eng.update(0.0f);
    CHECK(quota.activeCount == 1);
    CHECK(quota.releaseCalls == 3);

    eng.stopVoice();
    CHECK(quota.activeCount == 1);
    CHECK(quota.releaseCalls == 3);
    eng.stopVoice();
    CHECK(quota.activeCount == 0);
    CHECK(quota.releaseCalls == 4);
    CHECK(quota.releaseUnderflows == 0);
}

TEST_CASE("SoLoudAudioEngine retiring BGM releases only after physical voice ends") {
    AudioQuota quota(4);
    ScopedAudioQuota scopedQuota(quota);
    SoLoudAudioEngine eng;
    REQUIRE(eng.init());

    const unsigned int first = eng.playBGM("tests/audio/silence.wav", 0.0f);
    REQUIRE(first != 0);
    eng.soloud().setLooping(first, true);
    const unsigned int second = eng.playBGM("tests/audio/silence.wav", 10.0f);
    REQUIRE(second != 0);
    eng.soloud().setLooping(second, true);

    CHECK(quota.activeCount == 2);
    CHECK(quota.peakCount == 2);
    CHECK(quota.releaseCalls == 0);

    eng.soloud().stop(first);
    eng.update(0.0f);
    CHECK(quota.activeCount == 1);
    CHECK(quota.releaseCalls == 1);

    eng.stopBGM(0.0f);
    CHECK(quota.activeCount == 0);
    CHECK(quota.releaseCalls == 2);
    CHECK(quota.releaseUnderflows == 0);
}

TEST_CASE("SoLoudAudioEngine rapid BGM replacement is capped including retiring voices") {
    AudioQuota quota(3);
    ScopedAudioQuota scopedQuota(quota);
    SoLoudAudioEngine eng;
    REQUIRE(eng.init());

    const unsigned int first = eng.playBGM("tests/audio/silence.wav", 0.0f);
    REQUIRE(first != 0);
    eng.soloud().setLooping(first, true);
    const unsigned int second = eng.playBGM("tests/audio/silence.wav", 10.0f);
    REQUIRE(second != 0);
    eng.soloud().setLooping(second, true);
    const unsigned int current = eng.playBGM("tests/audio/silence.wav", 10.0f);
    REQUIRE(current != 0);
    eng.soloud().setLooping(current, true);

    REQUIRE(quota.activeCount == 3);
    const int activeVoices = eng.activeVoiceCount();
    CHECK(eng.playBGM("tests/audio/silence.wav", 10.0f) == 0);
    CHECK(eng.isBGMPlaying());
    CHECK(eng.activeVoiceCount() == activeVoices);
    CHECK(quota.activeCount == 3);
    CHECK(quota.tryCalls == 4);
    CHECK(quota.releaseCalls == 0);

    eng.stopBGM(10.0f);
    CHECK_FALSE(eng.soloud().isValidVoiceHandle(first));
    CHECK_FALSE(eng.soloud().isValidVoiceHandle(second));
    CHECK(eng.soloud().isValidVoiceHandle(current));
    CHECK(quota.activeCount == 1);
    CHECK(quota.releaseCalls == 2);

    eng.stopBGM(10.0f);
    CHECK_FALSE(eng.soloud().isValidVoiceHandle(current));
    CHECK(quota.activeCount == 0);
    CHECK(quota.releaseCalls == 3);
    CHECK(quota.releaseUnderflows == 0);
}

TEST_CASE("SoLoudAudioEngine shutdown releases all remaining handle quotas once") {
    AudioQuota quota(8);
    ScopedAudioQuota scopedQuota(quota);
    SoLoudAudioEngine eng;
    REQUIRE(eng.init());

    const unsigned int firstBGM = eng.playBGM("tests/audio/silence.wav", 0.0f);
    REQUIRE(firstBGM != 0);
    eng.soloud().setLooping(firstBGM, true);
    const unsigned int currentBGM = eng.playBGM("tests/audio/silence.wav", 10.0f);
    REQUIRE(currentBGM != 0);
    eng.soloud().setLooping(currentBGM, true);

    const unsigned int firstVoice = eng.playVoice("tests/audio/silence.wav");
    REQUIRE(firstVoice != 0);
    eng.soloud().setLooping(firstVoice, true);
    const unsigned int currentVoice = eng.playVoice("tests/audio/silence.wav");
    REQUIRE(currentVoice != 0);
    eng.soloud().setLooping(currentVoice, true);

    REQUIRE(eng.playSE("tests/audio/silence.wav") != 0);
    REQUIRE(eng.playSE3D("tests/audio/silence.wav", 0.0f, 0.0f, -1.0f) != 0);
    REQUIRE(quota.activeCount == 6);

    eng.shutdown();
    CHECK(quota.activeCount == 0);
    CHECK(quota.releaseCalls == 6);
    CHECK(quota.releaseUnderflows == 0);

    eng.shutdown();
    CHECK(quota.releaseCalls == 6);
}
