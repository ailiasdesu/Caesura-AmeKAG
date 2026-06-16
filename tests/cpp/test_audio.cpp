#include "doctest.h"
#include <cstring>
#include "audio/SoLoudAudioEngine.h"

using namespace Caesura;

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
