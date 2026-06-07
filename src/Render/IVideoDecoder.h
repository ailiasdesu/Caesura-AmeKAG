#pragma once
// @Beta: FFmpeg-based video decoder planned for Beta
// Alpha ships this interface as contract only — no implementations exist yet.
// See [10.2.17] IVideoDecoder extension interface in the spec.

class IVideoDecoder {
public:
    virtual ~IVideoDecoder() = default;

    /// Open a video file. Returns false if codec not supported.
    virtual bool open(const char* path) = 0;

    /// Decode next frame into caller-owned RGBA buffer.
    /// outW/outH report frame dimensions, outPts reports presentation timestamp.
    /// Returns false on EOF or decode error.
    virtual bool decodeFrame(unsigned char* outRGBA, int* outW, int* outH, double* outPts) = 0;

    /// Release all decoder resources.
    virtual void close() = 0;

    /// True while playback is active (not paused, not ended).
    virtual bool isPlaying() const = 0;

    /// True after the last frame has been decoded.
    virtual bool hasEnded() const = 0;
};