#pragma once
#include <cstdint>
#include <string>

namespace Caesura {

// ===========================================================================
// MiniLight — light sources for mini-game 3D scenes
// ===========================================================================
// Supports up to 4 lights total (1 directional + up to 3 point lights).
// The directional light is always active; point lights are additive.

enum class MiniLightType : uint8_t {
    Directional = 0,
    Point       = 1,
};

// Directional light: infinite distance, parallel rays
struct MiniDirectionalLight {
    float dirX = 0.577f, dirY = 0.577f, dirZ = 0.577f;
    float r = 1.0f, g = 1.0f, b = 1.0f;
    float intensity = 1.0f;  // multiplier
};

// Point light: position-based, distance attenuation
struct MiniPointLight {
    uint32_t id = 0;
    std::string name;
    float posX = 0, posY = 3, posZ = 0;
    float r = 1, g = 1, b = 1;
    float intensity = 1.0f;
    float range = 10.0f;     // max influence distance
};

// Ambient light (global)
struct MiniAmbientLight {
    float r = 0.15f, g = 0.15f, b = 0.15f;
};

} // namespace Caesura
