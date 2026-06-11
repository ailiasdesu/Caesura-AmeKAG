#pragma once
#include "api/IVideoPlayer.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <bgfx/bgfx.h>

namespace Caesura {

// -- DecodedFrame: worker -> main-thread transfer buffer -------------------
struct DecodedFrame {
    std::vector<uint8_t> rgba;
    int width  = 0;
    int height = 0;
    bool valid = false;
};

// ============================================================================
// VideoPlayer -- MPEG1/FFmpeg video playback, implements IVideoPlayer
// ============================================================================

class VideoPlayer : public IVideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer() override;

    VideoPlayer(const VideoPlayer&) = delete;
    VideoPlayer& operator=(const VideoPlayer&) = delete;

    VideoHandle open(const char* path) override;
    void close(VideoHandle handle) override;
    bool update(VideoHandle handle, double dt) override;
    bgfx::TextureHandle getTexture(VideoHandle handle) const override;

    bool isPlaying(VideoHandle handle) const override;
    bool hasEnded(VideoHandle handle) const override;
    int  width(VideoHandle handle) const override;
    int  height(VideoHandle handle) const override;
    double duration(VideoHandle handle) const override;
    double currentTime(VideoHandle handle) const override;

    void pause(VideoHandle handle) override;
    void resume(VideoHandle handle) override;
    void seek(VideoHandle handle, double time) override;

    void shutdown() override;
    int  activeCount() const override { return static_cast<int>(m_videos.size()); }

private:
    struct VideoState {
        void*  plm = nullptr;
        bool   useFFmpeg = false;
        bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
        int    width  = 0;
        int    height = 0;
        double duration = 0.0;
        bool   playing = true;
        bool   ended   = false;
        bool   hasFrame = false;
        std::shared_ptr<DecodedFrame> m_readyFrame;
        std::shared_ptr<DecodedFrame> m_nextFrame;

#ifdef CAESURA_VIDEO_FFMPEG
        void*  avFormat = nullptr;
        void*  avCodec  = nullptr;
        void*  avFrame  = nullptr;
        void*  avFrameRGB = nullptr;
        void*  swsCtx   = nullptr;
        int    videoStreamIndex = -1;
        std::vector<uint8_t> rgbaBuffer;
#endif
    };

    VideoState* find(VideoHandle handle);
    void destroyTexture(VideoState& vs);

    std::unordered_map<uint32_t, VideoState> m_videos;
    uint32_t m_nextId = 1;
};

} // namespace Caesura