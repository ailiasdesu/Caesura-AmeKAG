#pragma once
#include "render/api/IGpuMonitor.h"

namespace Caesura {

// NullGpuMonitor — no-op implementation for headless/test environments.
// Returns HIGH quality always, zero metrics, no degradation.
class NullGpuMonitor : public IGpuMonitor {
public:
    NullGpuMonitor() = default;
    ~NullGpuMonitor() override = default;

    GpuQuality update(double /*dt*/) override { return GpuQuality::HIGH; }
    const FrameMetrics& metrics() override { return m_metrics; }
    GpuQuality currentQuality() const override { return GpuQuality::HIGH; }
    bool isDegraded() const override { return false; }
    float resolutionScale() const override { return 1.0f; }
    bool vfxEnabled() const override { return true; }
    void reset() override { m_metrics = FrameMetrics{}; }

private:
    FrameMetrics m_metrics;
};

} // namespace Caesura
