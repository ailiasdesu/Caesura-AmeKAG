 #pragma once
#include <cstdint>
#include <cstddef>

namespace Caesura {

// ---------------------------------------------------------------------------
// Precompiled Vulkan SPIR-V bytecodes (Vulkan backend fallback)
// ---------------------------------------------------------------------------

extern const uint32_t kEmbeddedVS_Sprite[];
extern const size_t   kEmbeddedVS_SpriteSize;
extern const uint32_t kEmbeddedFS_Texture[];

// SPIR-V fullscreen effects (Vulkan)
extern const uint32_t kEmbeddedSPIRV_vs_fullscreen[];
extern const size_t   kEmbeddedSPIRV_vs_fullscreen_size;
extern const uint32_t kEmbeddedSPIRV_fs_blend[];
extern const size_t   kEmbeddedSPIRV_fs_blend_size;
extern const uint32_t kEmbeddedSPIRV_fs_transition[];
extern const size_t   kEmbeddedSPIRV_fs_transition_size;
extern const uint32_t kEmbeddedSPIRV_fs_vfx[];
extern const size_t   kEmbeddedSPIRV_fs_vfx_size;
extern const size_t   kEmbeddedFS_TextureSize;

// ---------------------------------------------------------------------------
// Precompiled DX11 DXBC bytecodes (Direct3D 11/12 backend fallback)
// Compiled from shaders/dx11/*.hlsl via fxc.exe
// ---------------------------------------------------------------------------

extern const uint8_t  kEmbeddedDXBC_VS_Sprite[];
extern const size_t   kEmbeddedDXBC_VS_Sprite_size;
extern const uint8_t  kEmbeddedDXBC_FS_Texture[];
extern const size_t   kEmbeddedDXBC_FS_Texture_size;

// ---------------------------------------------------------------------------
// Precompiled DX11 DXBC bytecodes -- blend / transition / VFX / fullscreen
// ---------------------------------------------------------------------------

extern const uint8_t  kEmbeddedDXBC_vs_fullscreen[];
extern const size_t   kEmbeddedDXBC_vs_fullscreen_size;
extern const uint8_t  kEmbeddedDXBC_fs_blend[];
extern const size_t   kEmbeddedDXBC_fs_blend_size;
extern const uint8_t  kEmbeddedDXBC_fs_transition[];
extern const size_t   kEmbeddedDXBC_fs_transition_size;
extern const uint8_t  kEmbeddedDXBC_fs_vfx[];
extern const size_t   kEmbeddedDXBC_fs_vfx_size;

// -- Stretch / Affine blit shaders (DXBC, for D3D11/D3D12 backends) ----------
extern const uint8_t  kEmbeddedDXBC_stretch_blt_vs[];
extern const size_t   kEmbeddedDXBC_stretch_blt_vs_size;
extern const uint8_t  kEmbeddedDXBC_stretch_blt_fs[];
extern const size_t   kEmbeddedDXBC_stretch_blt_fs_size;
extern const uint8_t  kEmbeddedDXBC_affine_blt_vs[];
extern const size_t   kEmbeddedDXBC_affine_blt_vs_size;
extern const uint8_t  kEmbeddedDXBC_affine_blt_fs[];
extern const size_t   kEmbeddedDXBC_affine_blt_fs_size;


// -- MiniGame 3D shaders (DXBC, for Phong-lit 3D mini-games) -----------------
extern const uint8_t  kEmbeddedDXBC_MiniGame_VS[];
extern const size_t   kEmbeddedDXBC_MiniGame_VS_size;
extern const uint8_t  kEmbeddedDXBC_MiniGame_FS[];
extern const size_t   kEmbeddedDXBC_MiniGame_FS_size;

// -- MiniGame 3D shaders (GLSL, for OpenGL/GLES backends) --------------------
extern const char*    kEmbeddedGLSL_MiniGame_VS;
extern const char*    kEmbeddedGLSL_MiniGame_FS;

// -- MiniGame 3D shaders (MSL, for Metal backend) ----------------------------
extern const char*    kEmbeddedMSL_MiniGame_VS;
extern const char*    kEmbeddedMSL_MiniGame_FS;

} // namespace Caesura
