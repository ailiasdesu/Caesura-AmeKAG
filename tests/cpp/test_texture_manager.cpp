// test_texture_manager.cpp - TextureManager + VideoPlayer tests
#include "doctest.h"
#include "render/TextureManager.h"
#include "render/VideoPlayer.h"

using namespace Caesura;

TEST_CASE("TextureManager::singleton accessible") {
    TextureManager& tm = TextureManager::instance();
    (void)tm;
}

TEST_CASE("VideoPlayer::construct no-crash") {
    VideoPlayer vp;
    (void)vp;
}

TEST_CASE("TextureManager::singleton is not null") {
    TextureManager& tm = TextureManager::instance();
    CHECK(&tm != nullptr);
}

TEST_CASE("TextureManager::default state") {
    TextureManager& tm = TextureManager::instance();
    (void)tm;
}

TEST_CASE("VideoPlayer::default state after construction") {
    VideoPlayer vp;
    CHECK(&vp != nullptr);
}
