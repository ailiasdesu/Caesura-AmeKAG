#pragma once
#include "api/IVideoPlayer.h"
#include "../job/api/IJobSystem.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
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

    void setJobSystem(IJobSystem& js) { m_jobSystem = &js; }

    VideoHandle open(const char* path) override;
    void close(VideoHandle handle) override;
    bool update(VideoHandle handle, double dt) override;
    void updateAll(double dt) override;
    uint32_t getTexture(VideoHandle handle) const override;

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

    // pl_mpeg audio callback dispatch (worker thread). void* keeps pl_mpeg
    // C types out of this header; the .cpp casts.
    void onAudioDecoded(void* plm, void* samples);

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
        // Audio (pl_mpeg path): decoded float PCM queued by the audio
        // callback and drained into the audio backend during update().
        bool   audioEnabled = false;
        int    sampleRate = 0;
        std::vector<float> audioQueue;
        unsigned int audioHandle = 0;
        std::vector<unsigned int> audioHandles;  // all live chunks (stop all on close/seek)
        bool   audioStarted = false;
        // Frame-rate pacing: playhead advances by dt each update; frames are
        // decoded until plm_get_time() catches up (bounded per call).
        double frameRate = 0.0;
        double playhead  = 0.0;
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
    // Drain queued decoded PCM into the audio backend (main thread).
    void drainAudio(VideoState& vs);

    std::unordered_map<uint32_t, VideoState> m_videos;
    uint32_t m_nextId = 1;
    IJobSystem* m_jobSystem = nullptr;
    std::mutex m_audioMutex;  // guards audioQueue across worker/main threads
};

} // namespace Caesura