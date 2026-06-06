#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <bgfx/bgfx.h>

namespace Caesura {

// -- VideoHandle -----------------------------------------------------------
struct VideoHandle {
    uint32_t id = 0;
    explicit operator bool() const { return id != 0; }
    bool operator==(const VideoHandle& o) const { return id == o.id; }
};

// -- VideoPlayer -----------------------------------------------------------
// MPEG1 video playback via pl_mpeg. Decodes one frame per engine frame.
// Audio from the video file is decoded but not mixed (SoLoud handles audio
// separately; video audio track is discarded for now per Alpha spec).
//
// Thread safety: main-thread only (bgfx texture creation).

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    VideoPlayer(const VideoPlayer&) = delete;
    VideoPlayer& operator=(const VideoPlayer&) = delete;

    // Open an MPEG1 (.mpg) file. Returns a handle or {0} on failure.
    VideoHandle open(const char* path);

    // Close and release all resources for a video.
    void close(VideoHandle handle);

    // Decode the next video frame. Call once per engine frame before getTexture().
    // dt = frame delta in seconds for PTS synchronization.
    // Returns true if a new frame was decoded (texture updated).
    bool update(VideoHandle handle, double dt);

    // Get the current bgfx texture handle for the decoded frame.
    // Valid until the next update() call.
    bgfx::TextureHandle getTexture(VideoHandle handle) const;

    // State queries
    bool isPlaying(VideoHandle handle) const;
    bool hasEnded(VideoHandle handle) const;
    int  width(VideoHandle handle) const;
    int  height(VideoHandle handle) const;
    double duration(VideoHandle handle) const;
    double currentTime(VideoHandle handle) const;

    // Control
    void pause(VideoHandle handle);
    void resume(VideoHandle handle);
    void seek(VideoHandle handle, double time);

    // Cleanup all videos (call before bgfx::shutdown)
    void shutdown();

    // Query active video count
    int activeCount() const { return static_cast<int>(m_videos.size()); }

private:
    struct VideoState {
        void*  plm = nullptr;             // plm_t* (opaque to avoid header dependency)
        bool   useFFmpeg = false;         // true when FFmpeg path is active
        bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
        int    width  = 0;
        int    height = 0;
        double duration = 0.0;
        bool   playing = true;
        bool   ended   = false;
        bool   hasFrame = false;

#ifdef CAESURA_VIDEO_FFMPEG
        // FFmpeg handles (void* to avoid leaking headers into every TU)
        void*  avFormat = nullptr;        // AVFormatContext*
        void*  avCodec  = nullptr;        // AVCodecContext*
        void*  avFrame  = nullptr;        // AVFrame*
        void*  avFrameRGB = nullptr;      // AVFrame* (RGBA)
        void*  swsCtx   = nullptr;        // SwsContext*
        int    videoStreamIndex = -1;
        std::vector<uint8_t> rgbaBuffer;  // pre-allocated RGBA buffer
#endif
    };

    VideoState* find(VideoHandle handle);
    void destroyTexture(VideoState& vs);

    std::unordered_map<uint32_t, VideoState> m_videos;
    uint32_t m_nextId = 1;
};

} // namespace Caesura
