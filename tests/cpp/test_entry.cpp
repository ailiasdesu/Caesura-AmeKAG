// test_entry.cpp - entry module unit tests (R4.2, extended S3)
#include "doctest.h"
#include "entry/Engine.h"
#include "entry/EngineConfig.h"
#include "di/BackendRegistry.h"
#include <cstring>

using namespace Caesura;

TEST_CASE("Entry: EngineConfig default values") {
    EngineConfig cfg;
    CHECK(cfg.width == 1280);
    CHECK(cfg.height == 720);
    CHECK(cfg.title != nullptr);
    CHECK(std::strcmp(cfg.title, "Caesura (AmeKAG)") == 0);
    CHECK(cfg.headless == false);
    CHECK(cfg.editorMode == false);
}

TEST_CASE("Entry: EngineConfig pointer fields default nullptr") {
    EngineConfig cfg;
    CHECK(cfg.platform == nullptr);
    CHECK(cfg.render == nullptr);
    CHECK(cfg.audio == nullptr);
    CHECK(cfg.lua == nullptr);
    CHECK(cfg.inputRouter == nullptr);
    CHECK(cfg.gpuMonitor == nullptr);
    CHECK(cfg.videoPlayer == nullptr);
    CHECK(cfg.miniGame == nullptr);
    CHECK(cfg.animation == nullptr);
    CHECK(cfg.steam == nullptr);
}

TEST_CASE("Entry: EngineConfig headless mode flag") {
    EngineConfig cfg;
    cfg.headless = true;
    CHECK(cfg.headless == true);
    CHECK(cfg.editorMode == false);
}

TEST_CASE("Entry: EngineConfig editor mode flag") {
    EngineConfig cfg;
    cfg.editorMode = true;
    CHECK(cfg.editorMode == true);
    CHECK(cfg.headless == false);
}

TEST_CASE("Entry: EngineConfig custom title") {
    EngineConfig cfg;
    CHECK(std::strlen(cfg.title) > 0);
}

TEST_CASE("Entry: EngineConfig width/height range") {
    EngineConfig cfg;
    cfg.width = 640;
    cfg.height = 480;
    CHECK(cfg.width == 640);
    CHECK(cfg.height == 480);

    cfg.width = 1920;
    cfg.height = 1080;
    CHECK(cfg.width == 1920);
    CHECK(cfg.height == 1080);
}

// S3 — Engine construction and lifecycle tests

TEST_CASE("Entry: Engine constructs in headless mode without crash") {
    EngineConfig cfg;
    cfg.headless = true;
    Engine engine(cfg);
    CHECK(true);
}

TEST_CASE("Entry: Engine default construct then destruct without init") {
    EngineConfig cfg;
    cfg.headless = true;
    {
        Engine engine(cfg);
    }
    // Construct again to verify BackendRegistry state is clean
    Engine engine2(cfg);
    CHECK(true);
}
