#pragma once

#include "api/IDisplayService.h"

namespace Caesura {

// Headless / tests: always reports the given fixed metrics (default zeros).
class NullDisplayService final : public IDisplayService {
public:
    explicit NullDisplayService(uint32_t width = 0, uint32_t height = 0)
        : m_width(width), m_height(height) {}

    DisplayMetrics currentMetrics() const override {
        DisplayMetrics m;
        m.logicalWidth = m_width;
        m.logicalHeight = m_height;
        m.pixelWidth = m_width;
        m.pixelHeight = m_height;
        return m;
    }

private:
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace Caesura
