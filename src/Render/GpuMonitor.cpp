#include "GpuMonitor.h"
#include "../Core/DebugManager.h"
#include <bgfx/bgfx.h>
#include <cmath>
#include <cstdio>

namespace Caesura {

const char* gpuQualityName(GpuQuality q) {
    switch (q) {
        case GpuQuality::HIGH:   return "HIGH";
        case GpuQuality::MEDIUM: return "MEDIUM";
        case GpuQuality::LOW:    return "LOW";
    }
    return "UNKNOWN";
}

GpuMonitor::GpuMonitor() {
    m_frameTimeWindow.resize(kWindowSize, 0.0);
}

GpuQuality GpuMonitor::update(double dt) {
    m_metrics.frameCount++;
    m_metrics.cpuTimeMs = dt * 1000.0;

    // Read GPU stats from bgfx
    const bgfx::Stats* stats = bgfx::getStats();
    
    // gpuTimeEnd is in microseconds, convert to milliseconds
    // gpuTimeEnd is in microseconds, convert to milliseconds.
    // bgfx may return INT64_MAX (0x7fffffffffffffff) when GPU timer is
    // unavailable (e.g. no timestamp query support, or first frames).
    // Clamp to a reasonable range: 0..1000ms.
    double frameGpuUs = static_cast<double>(stats->gpuTimeEnd);
    constexpr double kMaxGpuUs = 1'000'000.0;  // 1 second (1000ms)
    if (frameGpuUs < 0.0 || frameGpuUs > kMaxGpuUs) {
        frameGpuUs = dt * 1'000'000.0;  // fall back to CPU dt in microseconds
    }
    double gpuTimeMs = frameGpuUs / 1000.0;
    
    // Clamp: bgfx can report 0 in first frame or on some backends
    if (gpuTimeMs <= 0.0) gpuTimeMs = m_metrics.cpuTimeMs;
    
    m_metrics.gpuTimeMs = gpuTimeMs;

    // Rolling average (cumulative for first kWindowSize frames to avoid zero-bias)
    double oldest = m_frameTimeWindow.front();
    m_frameTimeWindow.pop_front();
    m_frameTimeWindow.push_back(gpuTimeMs);
    m_windowSum += gpuTimeMs - oldest;
    if (m_metrics.frameCount < kWindowSize) {
        m_metrics.rollingAvgMs = m_windowSum / static_cast<double>(m_metrics.frameCount);
    } else {
        m_metrics.rollingAvgMs = m_windowSum / static_cast<double>(kWindowSize);
    }

    // -- Degradation / Recovery state machine ------------------------------
    
    // Check if frame was over budget
    if (gpuTimeMs > kBudgetMs) {
        m_metrics.overloadFrames++;
        m_consecutiveOverBudget++;
        m_consecutiveGoodFrames = 0;
    } else {
        m_consecutiveOverBudget = 0;

        // Recovery: use a slightly looser threshold to avoid oscillation
        if (m_quality != GpuQuality::HIGH && gpuTimeMs < kBudgetMsLoose) {
            m_consecutiveGoodFrames++;
        } else {
            m_consecutiveGoodFrames = 0;
        }
    }

    // Degrade: 3+ consecutive frames over budget
    if (m_consecutiveOverBudget >= kDegradeFrames && m_quality != GpuQuality::LOW) {
        GpuQuality prev = m_quality;
        m_quality = static_cast<GpuQuality>(static_cast<int>(m_quality) + 1);
        m_consecutiveOverBudget = 0;
        m_consecutiveGoodFrames = 0;
        DEBUG_WARN(SubSys::Render, ErrCode::Ok,
                   "GPU degrade: %s -> %s (avg=%.1fms, gpu=%.1fms)",
                   gpuQualityName(prev), gpuQualityName(m_quality),
                   m_metrics.rollingAvgMs, gpuTimeMs);
    }

    // Recover: 10+ consecutive good frames
    if (m_consecutiveGoodFrames >= kRecoverFrames && m_quality != GpuQuality::HIGH) {
        GpuQuality prev = m_quality;
        m_quality = static_cast<GpuQuality>(static_cast<int>(m_quality) - 1);
        m_consecutiveGoodFrames = 0;
        DEBUG_INFO(SubSys::Render, ErrCode::Ok,
                   "GPU recover: %s -> %s (avg=%.1fms)",
                   gpuQualityName(prev), gpuQualityName(m_quality),
                   m_metrics.rollingAvgMs);
    }

    m_metrics.quality  = m_quality;
    m_metrics.degraded = (m_quality != GpuQuality::HIGH);

    // Log quality changes every 300 frames or on change
    static int logCounter = 0;
    if (++logCounter >= 300 || m_consecutiveOverBudget >= kDegradeFrames) {
        logCounter = 0;
        DEBUG_DBG(SubSys::Render, ErrCode::Ok,
                  "GPU[%s] frame=%.1fms cpu=%.1fms avg=%.1fms draw=%u wait=%u",
                  gpuQualityName(m_quality), gpuTimeMs, m_metrics.cpuTimeMs,
                  m_metrics.rollingAvgMs,
                  stats->numDraw, stats->numCompute);
    }

    return m_quality;
}

float GpuMonitor::resolutionScale() const {
    switch (m_quality) {
        case GpuQuality::HIGH:   return 1.0f;
        case GpuQuality::MEDIUM: return 0.75f;
        case GpuQuality::LOW:    return 0.5f;
    }
    return 1.0f;
}

void GpuMonitor::reset() {
    m_quality = GpuQuality::HIGH;
    m_consecutiveOverBudget = 0;
    m_consecutiveGoodFrames = 0;
    m_windowSum = 0.0;
    m_frameTimeWindow.assign(kWindowSize, 0.0);
    m_metrics = FrameMetrics{};
}

} // namespace Caesura
