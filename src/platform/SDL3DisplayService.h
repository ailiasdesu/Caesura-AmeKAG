#pragma once

#include "api/IDisplayService.h"

namespace Caesura {

class IPlatformBackend;

// Desktop display metrics from SDL3. The window handle is queried lazily
// through IPlatformBackend::getNativeWindowHandle() on each call, so the
// service can be constructed before the platform backend has created its
// window (composition root order). No SDL dependency leaks into the header.
class SDL3DisplayService final : public IDisplayService {
public:
    explicit SDL3DisplayService(const IPlatformBackend* platform)
        : m_platform(platform) {}

    DisplayMetrics currentMetrics() const override;

private:
    const IPlatformBackend* m_platform = nullptr;
};

} // namespace Caesura
