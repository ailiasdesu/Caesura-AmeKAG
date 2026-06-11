#pragma once
#include <cstdint>

namespace Caesura {

enum class GpuQuality : uint8_t;
const char* gpuQualityName(GpuQuality q);

class IGpuMonitor {
public:
    virtual ~IGpuMonitor() = default;

    struct FrameMetrics {
        double gpuTimeMs    = 0.0;
        double cpuTimeMs    = 0.0;
        double rollingAvgMs = 0.0;
        uint32_t frameCount = 0;
        uint32_t overloadFrames = 0;
        bool     degraded    = false;
        GpuQuality quality   = GpuQuality::HIGH;
    };

    virtual GpuQuality update(double dt) = 0;
    virtual const FrameMetrics& metrics() = 0;
    virtual GpuQuality currentQuality() const = 0;
    virtual bool isDegraded() const = 0;
    virtual float resolutionScale() const = 0;
    virtual bool vfxEnabled() const = 0;
    virtual void reset() = 0;
};

} // namespace Caesura