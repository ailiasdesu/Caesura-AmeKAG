// Engine_Gpu.cpp — isolated GpuMonitor factory (F1)
// Separates the concrete GpuMonitor/NullGpuMonitor includes from Engine.cpp,
// keeping the composition root free of render-level implementation details.
#include "../render/GpuMonitor.h"
#include "../render/NullGpuMonitor.h"
#include "render/api/IGpuMonitor.h"
#include <memory>

namespace Caesura {

std::unique_ptr<IGpuMonitor> createGpuMonitor(bool headless) {
    if (headless)
        return std::make_unique<NullGpuMonitor>();
    return std::make_unique<GpuMonitor>();
}

} // namespace Caesura
