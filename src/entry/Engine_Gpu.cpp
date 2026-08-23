// Engine_Gpu.cpp — isolated GpuMonitor factory (F1)
// Separates the concrete GpuMonitor/NullGpuMonitor includes from Engine.cpp,
// keeping the composition root free of render-level implementation details.
#include "../render/GpuMonitor.h"
#include "../render/NullGpuMonitor.h"
#include "../platform/SDL3DisplayService.h"
#include "../platform/NullDisplayService.h"
#include "entry/EngineConfig.h"
#include <memory>

namespace Caesura {

std::unique_ptr<IGpuMonitor> createGpuMonitor(bool headless) {
    if (headless)
        return std::make_unique<NullGpuMonitor>();
    return std::make_unique<GpuMonitor>();
}

std::unique_ptr<IDisplayService> createDisplayService(const EngineConfig& config) {
    // Desktop GPU builds query live SDL3 metrics through the platform
    // backend; headless/tests get the fixed-zero Null implementation.
    if (config.platform != nullptr && !config.headless)
        return std::make_unique<SDL3DisplayService>(config.platform);
    return std::make_unique<NullDisplayService>(config.width, config.height);
}

} // namespace Caesura
