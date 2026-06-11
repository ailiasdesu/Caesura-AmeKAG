#pragma once

#include <bgfx/bgfx.h>
#include <string>

namespace Caesura {

// BgfxShaderManager — owns all bgfx shader programs and uniforms.
// Extracted from BgfxRenderDevice during interface isolation (U7).
// Deployed as a member of BgfxRenderDevice; the device delegates
// program/uniform access through this class.

class BgfxShaderManager {
public:
    BgfxShaderManager() = default;
    ~BgfxShaderManager();

    BgfxShaderManager(const BgfxShaderManager&) = delete;
    BgfxShaderManager& operator=(const BgfxShaderManager&) = delete;

    // Load embedded shaders for the current bgfx renderer backend.
    // Returns true on success.
    bool initEmbeddedShaders();

    // -- Program handle accessors --
    bgfx::ProgramHandle getFallbackProgram()    const { return m_fallbackProgram; }
    bgfx::ProgramHandle getBlendProgram()       const { return m_blendProgram; }
    bgfx::ProgramHandle getTransitionProgram()  const { return m_transitionProgram; }
    bgfx::ProgramHandle getVFXProgram()         const { return m_vfxProgram; }
    bgfx::ProgramHandle getStretchProgram()     const { return m_stretchProgram; }
    bgfx::ProgramHandle getAffineProgram()      const { return m_affineProgram; }

    // -- Uniform handle accessors --
    bgfx::UniformHandle getDefaultSampler()    const { return m_texSampler; }
    bgfx::UniformHandle getBlendParams()       const { return m_u_blendParams; }
    bgfx::UniformHandle getTransParams()       const { return m_u_transParams; }
    bgfx::UniformHandle getVFXParams()         const { return m_u_vfxParams; }
    bgfx::UniformHandle getStretchParams()     const { return m_u_stretchParams; }
    bgfx::UniformHandle getAffineParams()      const { return m_u_affineParams; }

private:
    bgfx::ProgramHandle m_fallbackProgram    = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_blendProgram       = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_transitionProgram  = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_vfxProgram         = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_stretchProgram     = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_affineProgram      = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle m_texSampler         = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_blendParams      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_transParams      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_vfxParams        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_stretchParams    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_affineParams     = BGFX_INVALID_HANDLE;
};

} // namespace Caesura
