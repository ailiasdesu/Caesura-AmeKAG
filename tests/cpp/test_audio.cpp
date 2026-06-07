#include "doctest.h"
#include "Audio/SoLoudAudioEngine.h"

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
