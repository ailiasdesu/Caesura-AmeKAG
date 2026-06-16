// test_texture_manager.cpp - TextureManager + VideoPlayer tests
#include "doctest.h"
#include "render/TextureManager.h"
#include "render/VideoPlayer.h"

using namespace Caesura;

// =============================================================================
// TextureManager — non-GPU methods (safe per AGENTS.md section 8)
// =============================================================================

TEST_CASE("TextureManager::singleton") {
    auto& a = TextureManager::instance();
    auto& b = TextureManager::instance();
    CHECK(&a == &b);
}



TEST_CASE("TextureManager::isValid rejects invalid IDs") {
    auto& tm = TextureManager::instance();
    CHECK_FALSE(tm.isValid(0));
    CHECK_FALSE(tm.isValid(99999));
    CHECK_FALSE(tm.isValid(UINT32_MAX));
}

TEST_CASE("TextureManager::getTextureHandle for nonexistent ID") {
    auto& tm = TextureManager::instance();
    CHECK(tm.getTextureHandle(0) == 0);
    CHECK(tm.getTextureHandle(99999) == 0);
}

TEST_CASE("TextureManager::getTextureSizeById for nonexistent ID") {
    auto& tm = TextureManager::instance();
    uint16_t w = 1, h = 1;
    tm.getTextureSizeById(0, w, h);
    // Should not crash; w/h may be zeroed or unchanged
}



// =============================================================================
// VideoPlayer
// =============================================================================

TEST_CASE("VideoPlayer::construct no-crash") {
    VideoPlayer vp;
    (void)vp;
}

TEST_CASE("VideoPlayer::default state") {
    VideoPlayer vp;
    CHECK_FALSE(vp.isPlaying(VideoHandle{}));
    // hasEnded with invalid handle is implementation-defined
}
