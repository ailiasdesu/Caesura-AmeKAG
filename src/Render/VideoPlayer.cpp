#include <cstdio>
#define PL_MPEG_IMPLEMENTATION
#include "../../external/pl_mpeg/pl_mpeg.h"
#include "VideoPlayer.h"
#include "../Core/DebugManager.h"
#include <cmath>
#include <vector>

namespace Caesura {

VideoPlayer::VideoPlayer()  = default;
VideoPlayer::~VideoPlayer() { shutdown(); }

VideoHandle VideoPlayer::open(const char* path) {
    plm_t* plm = plm_create_with_filename(path);
    if (!plm) {
        DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                  "VideoPlayer: failed to open '%s'", path);
        return VideoHandle{};
    }

    VideoState vs;
    vs.plm      = plm;
    vs.width    = plm_get_width(plm);
    vs.height   = plm_get_height(plm);
    vs.duration = plm_get_duration(plm);
    vs.playing  = true;
    vs.ended    = false;
    vs.hasFrame = false;

    vs.texture = bgfx::createTexture2D(
        (uint16_t)vs.width, (uint16_t)vs.height,
        false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT
    );

    if (!bgfx::isValid(vs.texture)) {
        DEBUG_ERR(SubSys::Render, ErrCode::Render_TextureCreateFailed,
                  "VideoPlayer: texture creation failed %dx%d", vs.width, vs.height);
        plm_destroy(plm);
        return VideoHandle{};
    }

    VideoHandle handle{ m_nextId++ };
    m_videos[handle.id] = vs;

    DEBUG_INFO(SubSys::Render, ErrCode::Ok,
               "VideoPlayer: opened '%s' %dx%d %.1fs (id=%u)",
               path, vs.width, vs.height, vs.duration, handle.id);
    return handle;
}

void VideoPlayer::close(VideoHandle handle) {
    VideoState* vs = find(handle);
    if (!vs) return;

    destroyTexture(*vs);
    if (vs->plm) {
        plm_destroy(static_cast<plm_t*>(vs->plm));
        vs->plm = nullptr;
    }
    m_videos.erase(handle.id);
}

bool VideoPlayer::update(VideoHandle handle, double dt) {
    (void)dt;
    VideoState* vs = find(handle);
    if (!vs || !vs->playing || vs->ended) return false;

    plm_t* plm = static_cast<plm_t*>(vs->plm);

    plm_frame_t* frame = plm_decode_video(plm);
    plm_decode_audio(plm);

    if (!frame) {
        if (plm_has_ended(plm)) {
            vs->ended = true;
            vs->playing = false;
            DEBUG_INFO(SubSys::Render, ErrCode::Ok,
                       "VideoPlayer: video %u ended", handle.id);
        }
        return false;
    }

    int stride = vs->width * 4;
    std::vector<uint8_t> rgba(static_cast<size_t>(vs->width) * vs->height * 4);
    plm_frame_to_rgba(frame, rgba.data(), stride);

    const bgfx::Memory* mem = bgfx::copy(rgba.data(), (uint32_t)rgba.size());
    bgfx::updateTexture2D(vs->texture, 0, 0, 0, 0,
                          (uint16_t)vs->width, (uint16_t)vs->height, mem);

    vs->hasFrame = true;
    return true;
}

bgfx::TextureHandle VideoPlayer::getTexture(VideoHandle handle) const {
    auto it = m_videos.find(handle.id);
    if (it == m_videos.end() || !it->second.hasFrame)
        return BGFX_INVALID_HANDLE;
    return it->second.texture;
}

bool VideoPlayer::isPlaying(VideoHandle handle) const {
    auto it = m_videos.find(handle.id);
    return it != m_videos.end() && it->second.playing && !it->second.ended;
}

bool VideoPlayer::hasEnded(VideoHandle handle) const {
    auto it = m_videos.find(handle.id);
    return it == m_videos.end() || it->second.ended;
}

int VideoPlayer::width(VideoHandle handle) const {
    auto it = m_videos.find(handle.id);
    return it != m_videos.end() ? it->second.width : 0;
}

int VideoPlayer::height(VideoHandle handle) const {
    auto it = m_videos.find(handle.id);
    return it != m_videos.end() ? it->second.height : 0;
}

double VideoPlayer::duration(VideoHandle handle) const {
    auto it = m_videos.find(handle.id);
    return it != m_videos.end() ? it->second.duration : 0.0;
}

double VideoPlayer::currentTime(VideoHandle handle) const {
    auto it = m_videos.find(handle.id);
    if (it == m_videos.end() || !it->second.plm) return 0.0;
    return plm_get_time(static_cast<plm_t*>(it->second.plm));
}

void VideoPlayer::pause(VideoHandle handle) {
    VideoState* vs = find(handle);
    if (vs) vs->playing = false;
}

void VideoPlayer::resume(VideoHandle handle) {
    VideoState* vs = find(handle);
    if (vs && !vs->ended) vs->playing = true;
}

void VideoPlayer::seek(VideoHandle handle, double time) {
    VideoState* vs = find(handle);
    if (!vs || !vs->plm) return;
    plm_seek(static_cast<plm_t*>(vs->plm), time, 0);
    vs->hasFrame = false;
}

void VideoPlayer::shutdown() {
    for (auto& [id, vs] : m_videos) {
        destroyTexture(vs);
        if (vs.plm) {
            plm_destroy(static_cast<plm_t*>(vs.plm));
            vs.plm = nullptr;
        }
    }
    m_videos.clear();
}

VideoPlayer::VideoState* VideoPlayer::find(VideoHandle handle) {
    auto it = m_videos.find(handle.id);
    return it != m_videos.end() ? &it->second : nullptr;
}

void VideoPlayer::destroyTexture(VideoState& vs) {
    if (bgfx::isValid(vs.texture)) {
        bgfx::destroy(vs.texture);
        vs.texture = BGFX_INVALID_HANDLE;
    }
}

} // namespace Caesura