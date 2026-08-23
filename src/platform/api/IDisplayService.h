#pragma once

#include <cstdint>

namespace Caesura {

// ---------------------------------------------------------------------------
// IDisplayService   Unified display metrics across desktop / web / mobile
// ---------------------------------------------------------------------------
// Track P (P1 Display Service): one place to ask "what is the screen right
// now" without platform ifdefs. Implementations:
//   * SDL3DisplayService  — desktop (SDL3 window/display queries)
//   * NullDisplayService  — headless / tests (fixed zero metrics)
// Android/iOS implementations land with Track M / Track I.
//
// Definitions live in the interface header because interface methods pass
// them by value (AGENTS.md §2). No data members on the interface itself.

enum class Orientation : uint8_t {
    Unknown = 0,
    Portrait,
    PortraitUpsideDown,
    LandscapeLeft,
    LandscapeRight,
};

// Safe-area insets (notch / rounded corners / home indicator) in LOGICAL
// pixels. Desktop and headless always report zeros; mobile UI needs real
// values once Track M/I land.
struct Insets {
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;
};

struct DisplayMetrics {
    // Pixel (physical) size of the drawable surface.
    uint32_t pixelWidth = 0;
    uint32_t pixelHeight = 0;
    // Logical (DPI-independent) size the game should lay out against.
    uint32_t logicalWidth = 0;
    uint32_t logicalHeight = 0;
    // Physical / logical ratio (1.0 = 96dpi-like baseline; 2.0 = retina/2x).
    double scaleFactor = 1.0;
    // Approximate effective DPI (96 * scaleFactor — SDL3 provides content
    // scale, not raw DPI, so this is the documented mapping).
    double dpi = 96.0;
    Orientation orientation = Orientation::Unknown;
    Insets safeArea{};
};

class IDisplayService {
public:
    virtual ~IDisplayService() = default;

    // Snapshot of the CURRENT display state. Cheap (SDL query); safe to call
    // every frame. Zero-initialized fields mean "not available" (headless).
    virtual DisplayMetrics currentMetrics() const = 0;
};

} // namespace Caesura
