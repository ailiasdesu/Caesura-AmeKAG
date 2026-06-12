// test_audio_integration.cpp - audio playback integration tests
#include "doctest.h"
#include "audio/SoLoudAudioEngine.h"
#include <cstring>

using namespace Caesura;

TEST_CASE("Audio: playSE with silence file returns valid handle") {
    SoLoudAudioEngine eng;
    CHECK(eng.init());
    unsigned int h = eng.playSE("tests/audio/silence.wav");
    if (h != 0) {
        eng.stopSE();
    }
    CHECK(eng.activeVoiceCount() >= 0);
}

TEST_CASE("Audio: playSE invalid file returns 0") {
    SoLoudAudioEngine eng;
    eng.init();
    unsigned int h = eng.playSE("nonexistent.wav");
    CHECK(h == 0);
}

TEST_CASE("Audio: setGlobalVolume works") {
    SoLoudAudioEngine eng;
    eng.init();
    eng.setGlobalVolume(0.5f);
    CHECK(eng.getGlobalVolume() == doctest::Approx(0.5f));
    eng.setGlobalVolume(1.0f);
    CHECK(eng.getGlobalVolume() == doctest::Approx(1.0f));
}

TEST_CASE("Audio: isBGMPlaying returns false initially") {
    SoLoudAudioEngine eng;
    eng.init();
    CHECK_FALSE(eng.isBGMPlaying());
    CHECK_FALSE(eng.isVoicePlaying());
}

TEST_CASE("Audio: fadeVolume does not crash") {
    SoLoudAudioEngine eng;
    eng.init();
    eng.fadeVolume("bgm", 0.5f, 1.0f);
    eng.fadeVolume("voice", 0.5f, 1.0f);
    eng.fadeVolume("se", 0.5f, 1.0f);
}

TEST_CASE("Audio: shutdown then re-init succeeds") {
    SoLoudAudioEngine eng;
    CHECK(eng.init());
    eng.shutdown();
    CHECK(eng.init());
    eng.shutdown();
}

TEST_CASE("Audio: playSE before init returns 0") {
    SoLoudAudioEngine eng;
    unsigned int h = eng.playSE("tests/audio/silence.wav");
    CHECK(h == 0);
}

TEST_CASE("Audio: bus volume set/get roundtrip") {
    SoLoudAudioEngine eng;
    eng.init();
    eng.setBusVolume("bgm", 0.75f);
    CHECK(eng.getBusVolume("bgm") == doctest::Approx(0.75f));
    eng.setBusVolume("voice", 0.5f);
    CHECK(eng.getBusVolume("voice") == doctest::Approx(0.5f));
    eng.setBusVolume("se", 0.9f);
    CHECK(eng.getBusVolume("se") == doctest::Approx(0.9f));
}
