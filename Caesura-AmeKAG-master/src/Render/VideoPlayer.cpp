#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#define PL_MPEG_IMPLEMENTATION
#include "../../external/pl_mpeg/pl_mpeg.h"
#include "VideoPlayer.h"
#include "../Core/DebugManager.h"

#ifdef CAESURA_VIDEO_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
#endif

namespace Caesura {

// -- Helper: determine if a path should use pl_mpeg ------------------------
static bool shouldUsePlmpeg(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext) return false;
    // case-insensitive comparison
    const char* lower = ext;
    return (strcmp(lower, ".mpg") == 0 || strcmp(lower, ".mpeg") == 0);
}

VideoPlayer::VideoPlayer()  = default;
VideoPlayer::~VideoPlayer() { shutdown(); }

VideoHandle VideoPlayer::open(const char* path) {
    bool usePl = shouldUsePlmpeg(path);

#ifdef CAESURA_VIDEO_FFMPEG
    if (!usePl) {
        // -------- FFmpeg path --------
        VideoState vs;
        vs.useFFmpeg = true;

        AVFormatContext* avFormat = nullptr;
        int ret = avformat_open_input(&avFormat, path, nullptr, nullptr);
        if (ret < 0) {
            char errbuf[256];
            av_strerror(ret, errbuf, sizeof(errbuf));
            DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                      "VideoPlayer: avformat_open_input failed '%s': %s", path, errbuf);
            return VideoHandle{};
        }
        vs.avFormat = avFormat;

        ret = avformat_find_stream_info(avFormat, nullptr);
        if (ret < 0) {
            DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                      "VideoPlayer: avformat_find_stream_info failed '%s'", path);
            avformat_close_input(&avFormat);
            return VideoHandle{};
        }

        // Find video stream
        int videoIdx = -1;
        for (unsigned i = 0; i < avFormat->nb_streams; i++) {
            if (avFormat->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoIdx = (int)i;
                break;
            }
        }
        if (videoIdx < 0) {
            DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                      "VideoPlayer: no video stream in '%s'", path);
            avformat_close_input(&avFormat);
            return VideoHandle{};
        }
        vs.videoStreamIndex = videoIdx;

        AVStream* vStream = avFormat->streams[videoIdx];
        const AVCodec* codec = avcodec_find_decoder(vStream->codecpar->codec_id);
        if (!codec) {
            DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                      "VideoPlayer: unsupported codec in '%s'", path);
            avformat_close_input(&avFormat);
            return VideoHandle{};
        }

        AVCodecContext* avCodec = avcodec_alloc_context3(codec);
        if (!avCodec) {
            DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                      "VideoPlayer: avcodec_alloc_context3 failed");
            avformat_close_input(&avFormat);
            return VideoHandle{};
        }
        vs.avCodec = avCodec;

        avcodec_parameters_to_context(avCodec, vStream->codecpar);
        avCodec->thread_count = 0; // auto thread count

        ret = avcodec_open2(avCodec, codec, nullptr);
        if (ret < 0) {
            DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                      "VideoPlayer: avcodec_open2 failed");
            avcodec_free_context(&avCodec);
            avformat_close_input(&avFormat);
            return VideoHandle{};
        }

        vs.width  = avCodec->width;
        vs.height = avCodec->height;
        vs.duration = (double)avFormat->duration / (double)AV_TIME_BASE;
        vs.playing  = true;
        vs.ended    = false;
        vs.hasFrame = false;

        // SwsContext for YUV → RGBA
        SwsContext* sws = sws_getContext(
            avCodec->width, avCodec->height, avCodec->pix_fmt,
            avCodec->width, avCodec->height, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!sws) {
            DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                      "VideoPlayer: sws_getContext failed");
            avcodec_free_context(&avCodec);
            avformat_close_input(&avFormat);
            return VideoHandle{};
        }
        vs.swsCtx = sws;

        AVFrame* avFrame = av_frame_alloc();
        AVFrame* avFrameRGB = av_frame_alloc();
        if (!avFrame || !avFrameRGB) {
            DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                      "VideoPlayer: av_frame_alloc failed");
            sws_freeContext(sws);
            av_frame_free(&avFrame);
            av_frame_free(&avFrameRGB);
            avcodec_free_context(&avCodec);
            avformat_close_input(&avFormat);
            return VideoHandle{};
        }
        vs.avFrame    = avFrame;
        vs.avFrameRGB = avFrameRGB;

        int rgbSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, avCodec->width, avCodec->height, 1);
        vs.rgbaBuffer.resize((size_t)rgbSize);
        av_image_fill_arrays(avFrameRGB->data, avFrameRGB->linesize,
                             vs.rgbaBuffer.data(), AV_PIX_FMT_RGBA,
                             avCodec->width, avCodec->height, 1);

        // bgfx texture
        vs.texture = bgfx::createTexture2D(
            (uint16_t)vs.width, (uint16_t)vs.height,
            false, 1,
            bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT);
        if (!bgfx::isValid(vs.texture)) {
            DEBUG_ERR(SubSys::Render, ErrCode::Render_TextureCreateFailed,
                      "VideoPlayer: texture creation failed %dx%d", vs.width, vs.height);
            sws_freeContext(sws);
            av_frame_free(&avFrame);
            av_frame_free(&avFrameRGB);
            avcodec_free_context(&avCodec);
            avformat_close_input(&avFormat);
            return VideoHandle{};
        }

        VideoHandle handle{ m_nextId++ };
        m_videos[handle.id] = vs;

        DEBUG_INFO(SubSys::Render, ErrCode::Ok,
                   "VideoPlayer: opened (FFmpeg) '%s' %dx%d %.1fs (id=%u)",
                   path, vs.width, vs.height, vs.duration, handle.id);
        return handle;
    }

    // Fall through to pl_mpeg path
#endif // CAESURA_VIDEO_FFMPEG

    // -------- pl_mpeg path --------
    plm_t* plm = plm_create_with_filename(path);
    if (!plm) {
        DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                  "VideoPlayer: failed to open '%s'", path);
        return VideoHandle{};
    }

    VideoState vs;
    vs.plm      = plm;
    vs.useFFmpeg = false;
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
    if (vs->useFFmpeg) {
#ifdef CAESURA_VIDEO_FFMPEG
        auto* sws = static_cast<SwsContext*>(vs->swsCtx);
        auto* f   = static_cast<AVFrame*>(vs->avFrame);
        auto* fRGB = static_cast<AVFrame*>(vs->avFrameRGB);
        auto* cc  = static_cast<AVCodecContext*>(vs->avCodec);
        auto* fmt = static_cast<AVFormatContext*>(vs->avFormat);

        if (sws)  sws_freeContext(sws);
        if (f)    av_frame_free(&f);
        if (fRGB) av_frame_free(&fRGB);
        if (cc)   avcodec_free_context(&cc);
        if (fmt)  avformat_close_input(&fmt);

        vs->swsCtx = nullptr;
        vs->avFrame = nullptr;
        vs->avFrameRGB = nullptr;
        vs->avCodec = nullptr;
        vs->avFormat = nullptr;
#endif
    } else {
        if (vs->plm) {
            plm_destroy(static_cast<plm_t*>(vs->plm));
            vs->plm = nullptr;
        }
    }
    m_videos.erase(handle.id);
}

bool VideoPlayer::update(VideoHandle handle, double dt) {
    (void)dt;
    VideoState* vs = find(handle);
    if (!vs || !vs->playing || vs->ended) return false;

    if (vs->useFFmpeg) {
#ifdef CAESURA_VIDEO_FFMPEG
        auto* fmt  = static_cast<AVFormatContext*>(vs->avFormat);
        auto* cc   = static_cast<AVCodecContext*>(vs->avCodec);
        auto* f    = static_cast<AVFrame*>(vs->avFrame);
        auto* fRGB = static_cast<AVFrame*>(vs->avFrameRGB);
        auto* sws  = static_cast<SwsContext*>(vs->swsCtx);

        // Read packets until we get a video frame
        AVPacket* pkt = av_packet_alloc();
        bool gotFrame = false;

        while (!gotFrame) {
            int ret = av_read_frame(fmt, pkt);
            if (ret == AVERROR_EOF) {
                // Flush decoder
                av_packet_unref(pkt);
                ret = avcodec_send_packet(cc, nullptr);
                if (ret >= 0) {
                    ret = avcodec_receive_frame(cc, f);
                    if (ret == 0) {
                        gotFrame = true;
                    }
                }
                vs->ended = true;
                vs->playing = false;
                DEBUG_INFO(SubSys::Render, ErrCode::Ok,
                           "VideoPlayer: video %u ended", handle.id);
                break;
            }
            if (ret < 0) {
                av_packet_unref(pkt);
                break;
            }

            if (pkt->stream_index == vs->videoStreamIndex) {
                ret = avcodec_send_packet(cc, pkt);
                if (ret >= 0) {
                    ret = avcodec_receive_frame(cc, f);
                    if (ret == 0) {
                        gotFrame = true;
                    } else if (ret == AVERROR(EAGAIN)) {
                        // Need more packets - continue
                    }
                }
            }
            av_packet_unref(pkt);
        }

        av_packet_free(&pkt);

        if (gotFrame) {
            sws_scale(sws,
                      f->data, f->linesize, 0, cc->height,
                      fRGB->data, fRGB->linesize);

            const bgfx::Memory* mem = bgfx::copy(vs->rgbaBuffer.data(), (uint32_t)vs->rgbaBuffer.size());
            bgfx::updateTexture2D(vs->texture, 0, 0, 0, 0,
                                  (uint16_t)vs->width, (uint16_t)vs->height, mem);
            vs->hasFrame = true;
            return true;
        }
        return false;
#else
        return false;
#endif
    }

    // -------- pl_mpeg path --------
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
    if (it == m_videos.end()) return 0.0;

    if (it->second.useFFmpeg) {
#ifdef CAESURA_VIDEO_FFMPEG
        auto* cc = static_cast<AVCodecContext*>(it->second.avCodec);
        auto* f  = static_cast<AVFrame*>(it->second.avFrame);
        if (cc && f && f->pts != AV_NOPTS_VALUE) {
            AVRational tb = it->second.avFormat
                ? static_cast<AVFormatContext*>(it->second.avFormat)
                      ->streams[it->second.videoStreamIndex]->time_base
                : AVRational{1, 1};
            return (double)f->pts * av_q2d(tb);
        }
        return 0.0;
#else
        return 0.0;
#endif
    }
    if (!it->second.plm) return 0.0;
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
    if (!vs) return;

    if (vs->useFFmpeg) {
#ifdef CAESURA_VIDEO_FFMPEG
        auto* fmt = static_cast<AVFormatContext*>(vs->avFormat);
        auto* cc  = static_cast<AVCodecContext*>(vs->avCodec);
        int64_t ts = (int64_t)(time * (double)AV_TIME_BASE);
        int ret = av_seek_frame(fmt, -1, ts, AVSEEK_FLAG_BACKWARD);
        if (ret >= 0 && cc) {
            avcodec_flush_buffers(cc);
        }
#endif
    } else {
        if (vs->plm) {
            plm_seek(static_cast<plm_t*>(vs->plm), time, 0);
        }
    }
    vs->hasFrame = false;
}

void VideoPlayer::shutdown() {
    for (auto& [id, vs] : m_videos) {
        destroyTexture(vs);
        if (vs.useFFmpeg) {
#ifdef CAESURA_VIDEO_FFMPEG
            auto* sws = static_cast<SwsContext*>(vs.swsCtx);
            auto* f   = static_cast<AVFrame*>(vs.avFrame);
            auto* fRGB = static_cast<AVFrame*>(vs.avFrameRGB);
            auto* cc  = static_cast<AVCodecContext*>(vs.avCodec);
            auto* fmt = static_cast<AVFormatContext*>(vs.avFormat);

            if (sws)  sws_freeContext(sws);
            if (f)    av_frame_free(&f);
            if (fRGB) av_frame_free(&fRGB);
            if (cc)   avcodec_free_context(&cc);
            if (fmt)  avformat_close_input(&fmt);

            vs.swsCtx = nullptr;
            vs.avFrame = nullptr;
            vs.avFrameRGB = nullptr;
            vs.avCodec = nullptr;
            vs.avFormat = nullptr;
#endif
        } else {
            if (vs.plm) {
                plm_destroy(static_cast<plm_t*>(vs.plm));
                vs.plm = nullptr;
            }
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
