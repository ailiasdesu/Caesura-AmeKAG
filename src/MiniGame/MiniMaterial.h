#pragma once
#include <cstdint>
#include <string>

namespace Caesura {

// ===========================================================================
// MiniMaterial — PBR-lite material for mini-game 3D objects
// ===========================================================================
// Mirrors the shader''s u_material uniform (roughness/metallic/specStr).
// Default: white matte plastic (roughness 0.5, metallic 0.0)

struct MiniMaterial {
    uint32_t id = 0;
    std::string name;

    // Albedo (base color)
    float r = 1.0f, g = 1.0f, b = 1.0f;

    // PBR-lite parameters (maps to shader u_material)
    float roughness = 0.5f;   // 0=smooth mirror, 1=fully rough
    float metallic  = 0.0f;   // 0=dielectric, 1=metal
    float specular  = 0.5f;   // specular contribution strength

    // Emissive (self-illumination, additive)
    float emissiveR = 0.0f, emissiveG = 0.0f, emissiveB = 0.0f;
};

} // namespace Caesura
