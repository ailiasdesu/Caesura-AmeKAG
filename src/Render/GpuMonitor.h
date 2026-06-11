#pragma once
#include "api/IGpuMonitor.h"
#include <deque>
#include <deque>

namespace Caesura {

// -- Quality levels for adaptive degradation -------------------------------
// GpuQuality enum defined in api/IGpuMonitor.h





const char* gpuQualityName(GpuQuality q);

// -- GpuMonitor ------------------------------------------------------------
// Tracks GPU frame times via bgfx::getStats() and triggers adaptive quality
// degradation when 3+ consecutive frames exceed the budget threshold.
//
// Budget: 16.67ms @ 60fps for Intel HD 620 target.
// Recovery: 10 consecutive good frames to step up one quality level.

class GpuMonitor : public IGpuMonitor {
public:
    struct FrameMetrics {
        double gpuTimeMs    = 0.0;   // GPU frame time (bgfx stats)
        double cpuTimeMs    = 0.0;   // CPU frame time
        double rollingAvgMs = 0.0;   // Rolling average over window
        uint32_t frameCount = 0;     // Total frames since init
        uint32_t overloadFrames = 0; // Frames over budget
        bool     degraded    = false; // Currently at reduced quality
        GpuQuality quality   = GpuQuality::HIGH;
    };

    GpuMonitor();
    ~GpuMonitor() = default;

    // Call once per frame, before render submission.
    // dt = frame delta in seconds. Returns the current quality level.
    GpuQuality update(double dt) override;

    // Accessors
    const FrameMetrics& metrics() override { return m_metrics; }
    GpuQuality currentQuality() const override { return m_quality; }
    bool isDegraded() const override { return m_quality != GpuQuality::HIGH; }

    // Resolution scale factor for current quality level
    float resolutionScale() const override;

    // Whether VFX (blur, particles, transition shaders) should be disabled
    bool vfxEnabled() const override { return m_quality != GpuQuality::LOW; }

    // Reset counters (e.g. after scene change)
    void reset() override;

private:
    static constexpr double kBudgetMs      = 16.67;  // 60fps target
    static constexpr double kBudgetMsLoose = 20.0;   // Recovery threshold
    static constexpr int    kWindowSize    = 60;      // Rolling average window
    static constexpr int    kDegradeFrames = 3;       // Consecutive over-budget to degrade
    static constexpr int    kRecoverFrames = 10;      // Consecutive good frames to recover

    GpuQuality m_quality = GpuQuality::HIGH;
    FrameMetrics m_metrics;

    std::deque<double> m_frameTimeWindow;  // Last N GPU frame times (ms)
    double m_windowSum = 0.0;

    int m_consecutiveOverBudget = 0;   // Degradation counter
    int m_consecutiveGoodFrames = 0;   // Recovery counter
};

} // namespace Caesura
