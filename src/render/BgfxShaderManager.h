#pragma once

#include <bgfx/bgfx.h>
#include <string>

namespace Caesura {

class BgfxShaderManager {
public:
    BgfxShaderManager() = default;
    ~BgfxShaderManager();

    BgfxShaderManager(const BgfxShaderManager&) = delete;
    BgfxShaderManager& operator=(const BgfxShaderManager&) = delete;

    void initEmbeddedShaders();

    bgfx::ProgramHandle getFallbackProgram()    const { return m_fallbackProgram; }
    bgfx::ProgramHandle getBlendProgram()       const { return m_blendProgram; }
    bgfx::ProgramHandle getTransitionProgram()  const { return m_transitionProgram; }
    bgfx::ProgramHandle getVFXProgram()         const { return m_vfxProgram; }
    bgfx::ProgramHandle getStretchProgram()     const { return m_stretchProgram; }
    bgfx::ProgramHandle getAffineProgram()      const { return m_affineProgram; }
    bgfx::UniformHandle getDefaultSampler() const {
        if (!bgfx::isValid(m_texSampler))
            m_texSampler = bgfx::createUniform("s_texture", bgfx::UniformType::Sampler);
        return m_texSampler;
    }
    bgfx::UniformHandle getSampler1() const {
        if (!bgfx::isValid(m_texSampler1))
            m_texSampler1 = bgfx::createUniform("s_texture1", bgfx::UniformType::Sampler);
        return m_texSampler1;
    }
    bgfx::UniformHandle getSampler2() const {
        if (!bgfx::isValid(m_texSampler2))
            m_texSampler2 = bgfx::createUniform("s_texture2", bgfx::UniformType::Sampler);
        return m_texSampler2;
    }
    bgfx::UniformHandle getBlendParams()        const { return m_u_blendParams; }
    bgfx::UniformHandle getTransParams()        const { return m_u_transParams; }
    bgfx::UniformHandle getVFXParams()          const { return m_u_vfxParams; }
    bgfx::UniformHandle getStretchParams()      const { return m_u_stretchParams; }
    bgfx::UniformHandle getAffineParams()       const { return m_u_affineParams; }

private:
    bgfx::ProgramHandle m_fallbackProgram    = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_blendProgram       = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_transitionProgram  = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_vfxProgram         = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_stretchProgram     = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_affineProgram      = BGFX_INVALID_HANDLE;
    mutable bgfx::UniformHandle m_texSampler  = BGFX_INVALID_HANDLE;
    mutable bgfx::UniformHandle m_texSampler1 = BGFX_INVALID_HANDLE;
    mutable bgfx::UniformHandle m_texSampler2 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_blendParams      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_transParams      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_vfxParams        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_stretchParams    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_affineParams     = BGFX_INVALID_HANDLE;
    bool m_embeddedInit = false;  // per-instance initEmbeddedShaders guard
};

} // namespace Caesura