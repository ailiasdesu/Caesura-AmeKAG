#include "BgfxShaderManager.h"
#include "EmbeddedShaders.h"
#include "ShaderCache.h"
#include <bx/bx.h>
#include <bx/readerwriter.h>
#include <bx/error.h>
#include <cstdio>
#include <cstring>

namespace Caesura {

BgfxShaderManager::~BgfxShaderManager() {
    if (bgfx::isValid(m_fallbackProgram))    bgfx::destroy(m_fallbackProgram);
    if (bgfx::isValid(m_blendProgram))       bgfx::destroy(m_blendProgram);
    if (bgfx::isValid(m_transitionProgram))  bgfx::destroy(m_transitionProgram);
    if (bgfx::isValid(m_vfxProgram))         bgfx::destroy(m_vfxProgram);
    if (bgfx::isValid(m_stretchProgram))     bgfx::destroy(m_stretchProgram);
    if (bgfx::isValid(m_affineProgram))      bgfx::destroy(m_affineProgram);
    if (bgfx::isValid(m_texSampler))         bgfx::destroy(m_texSampler);
    if (bgfx::isValid(m_texSampler1))        bgfx::destroy(m_texSampler1);
    if (bgfx::isValid(m_texSampler2))        bgfx::destroy(m_texSampler2);
    if (bgfx::isValid(m_u_blendParams))      bgfx::destroy(m_u_blendParams);
    if (bgfx::isValid(m_u_transParams))      bgfx::destroy(m_u_transParams);
    if (bgfx::isValid(m_u_vfxParams))        bgfx::destroy(m_u_vfxParams);
    if (bgfx::isValid(m_u_stretchParams))    bgfx::destroy(m_u_stretchParams);
    if (bgfx::isValid(m_u_affineParams))     bgfx::destroy(m_u_affineParams);
}

struct ShaderUniformMetadata {
    const char* name = nullptr;
    uint8_t type = 0;
    uint8_t num = 0;
    uint16_t regIndex = 0;
    uint16_t regCount = 0;
    uint16_t constantBufferSize = 0;
};

static bgfx::ShaderHandle buildBgfxShader(
    const uint8_t* bytecode,
    uint32_t codeSize,
    bool fragment,
    uint8_t numAttrs,
    const uint16_t* attrIds,
    const ShaderUniformMetadata* uniform = nullptr) {
    // [10.2.3] Shader safety: reject bytecode > 64 KB (SPIR-V/DXBC limit)
    if (codeSize > 65536) {
        fprintf(stderr, "[BgfxShaderManager] Shader rejected: %u bytes exceeds 64 KB limit.\n", codeSize);
        return BGFX_INVALID_HANDLE;
    }

    const uint16_t uniformCount = uniform ? 1 : 0;
    const uint32_t uniformSize = uniform
        ? 1 + uint32_t(std::strlen(uniform->name)) + 1 + 1 + 2 + 2 + 1 + 1 + 2
        : 0;

    const uint32_t totalSize = 4 + 4 + 4 + 2
                             + uniformSize
                             + 4 + codeSize + 1
                             + 1 + 2 * numAttrs
                             + 2;

    const bgfx::Memory* mem = bgfx::alloc(totalSize);
    if (!mem) {
        fprintf(stderr, "[BgfxShaderManager] bgfx::alloc failed for %u bytes\n", totalSize);
        return BGFX_INVALID_HANDLE;
    }
    bx::StaticMemoryBlockWriter writer(mem->data, mem->size);
    bx::ErrorAssert err;

    const uint32_t magic = fragment
        ? BX_MAKEFOURCC('F', 'S', 'H', 11)
        : BX_MAKEFOURCC('V', 'S', 'H', 11);
    bx::write(&writer, magic, err);
    bx::write(&writer, uint32_t(0), err);
    bx::write(&writer, uint32_t(0), err);
    bx::write(&writer, uniformCount, err);

    if (uniform) {
        const uint8_t nameSize = uint8_t(std::strlen(uniform->name));
        bx::write(&writer, nameSize, err);
        for (uint8_t i = 0; i < nameSize; ++i)
            bx::write(&writer, uniform->name[i], err);
        bx::write(&writer, uniform->type, err);
        bx::write(&writer, uniform->num, err);
        bx::write(&writer, uniform->regIndex, err);
        bx::write(&writer, uniform->regCount, err);
        bx::write(&writer, uint8_t(0), err);
        bx::write(&writer, uint8_t(0), err);
        bx::write(&writer, uint16_t(0), err);
    }

    bx::write(&writer, codeSize, err);
    for (uint32_t i = 0; i < codeSize; ++i)
        bx::write(&writer, bytecode[i], err);
    bx::write(&writer, uint8_t(0), err);

    bx::write(&writer, numAttrs, err);
    for (uint8_t i = 0; i < numAttrs; ++i)
        bx::write(&writer, attrIds[i], err);

    bx::write(&writer, uniform ? uniform->constantBufferSize : uint16_t(0), err);

    return bgfx::createShader(mem);
}


// initEmbeddedShaders
// Picks the correct embedded bytecode (SPIR-V for Vulkan, DXBC for
// D3D11/D3D12), wraps it in a proper bgfx binary header via
// buildBgfxShader(), and registers the resulting program as the
// engine-wide fallback for 2-D quad rendering and RTT blits.

void BgfxShaderManager::initEmbeddedShaders() {
    static bool s_initialized = false;
    if (s_initialized) return;
    s_initialized = true;

    const bgfx::RendererType::Enum renderer = bgfx::getCaps()->rendererType;

    printf("[BgfxShaderManager] initEmbeddedShaders: renderer=%s\n",
           bgfx::getRendererName(renderer));
    // Initialize ShaderCache before registering any programs
    CompositeShaderCache::instance().init();

    struct Bytecode {
        const uint8_t* data = nullptr;
        size_t size = 0;
    };

    // Per-renderer bytecode selection. SPIR-V arrays are stored as dwords.
    const bool isVulkan = renderer == bgfx::RendererType::Vulkan;
    const bool isD3D = renderer == bgfx::RendererType::Direct3D11 ||
                       renderer == bgfx::RendererType::Direct3D12;
    const bool isGL = renderer == bgfx::RendererType::OpenGL ||
                      renderer == bgfx::RendererType::OpenGLES;
    const bool isMetal = renderer == bgfx::RendererType::Metal;

    Bytecode vsSprite, fsTexture, vsFullscreen, fsBlend, fsTransition, fsVfx;
    Bytecode stretchVs, stretchFs, affineVs, affineFs;

    if (isVulkan) {
        vsSprite   = { reinterpret_cast<const uint8_t*>(kEmbeddedVS_Sprite),
                       kEmbeddedVS_SpriteSize * sizeof(uint32_t) };
        fsTexture  = { reinterpret_cast<const uint8_t*>(kEmbeddedFS_Texture),
                       kEmbeddedFS_TextureSize * sizeof(uint32_t) };
        vsFullscreen = { reinterpret_cast<const uint8_t*>(kEmbeddedSPIRV_vs_fullscreen),
                         kEmbeddedSPIRV_vs_fullscreen_size * sizeof(uint32_t) };
        fsBlend    = { reinterpret_cast<const uint8_t*>(kEmbeddedSPIRV_fs_blend),
                       kEmbeddedSPIRV_fs_blend_size * sizeof(uint32_t) };
        fsTransition = { reinterpret_cast<const uint8_t*>(kEmbeddedSPIRV_fs_transition),
                         kEmbeddedSPIRV_fs_transition_size * sizeof(uint32_t) };
        fsVfx      = { reinterpret_cast<const uint8_t*>(kEmbeddedSPIRV_fs_vfx),
                       kEmbeddedSPIRV_fs_vfx_size * sizeof(uint32_t) };
    } else if (isD3D) {
        vsSprite   = { kEmbeddedDXBC_VS_Sprite, kEmbeddedDXBC_VS_Sprite_size };
        fsTexture  = { kEmbeddedDXBC_FS_Texture, kEmbeddedDXBC_FS_Texture_size };
        vsFullscreen = { kEmbeddedDXBC_vs_fullscreen, kEmbeddedDXBC_vs_fullscreen_size };
        fsBlend    = { kEmbeddedDXBC_fs_blend, kEmbeddedDXBC_fs_blend_size };
        fsTransition = { kEmbeddedDXBC_fs_transition, kEmbeddedDXBC_fs_transition_size };
        fsVfx      = { kEmbeddedDXBC_fs_vfx, kEmbeddedDXBC_fs_vfx_size };
        stretchVs  = { kEmbeddedDXBC_stretch_blt_vs, kEmbeddedDXBC_stretch_blt_vs_size };
        stretchFs  = { kEmbeddedDXBC_stretch_blt_fs, kEmbeddedDXBC_stretch_blt_fs_size };
        affineVs   = { kEmbeddedDXBC_affine_blt_vs, kEmbeddedDXBC_affine_blt_vs_size };
        affineFs   = { kEmbeddedDXBC_affine_blt_fs, kEmbeddedDXBC_affine_blt_fs_size };
    } else if (isGL) {
        vsSprite   = { kEmbeddedGL_vs_sprite, kEmbeddedGL_vs_sprite_size };
        fsTexture  = { kEmbeddedGL_fs_texture, kEmbeddedGL_fs_texture_size };
        vsFullscreen = { kEmbeddedGL_vs_fullscreen, kEmbeddedGL_vs_fullscreen_size };
        fsBlend    = { kEmbeddedGL_fs_blend, kEmbeddedGL_fs_blend_size };
        fsTransition = { kEmbeddedGL_fs_transition, kEmbeddedGL_fs_transition_size };
        fsVfx      = { kEmbeddedGL_fs_vfx, kEmbeddedGL_fs_vfx_size };
        stretchVs  = { kEmbeddedGL_stretch_blt_vs, kEmbeddedGL_stretch_blt_vs_size };
        stretchFs  = { kEmbeddedGL_stretch_blt_fs, kEmbeddedGL_stretch_blt_fs_size };
        affineVs   = { kEmbeddedGL_affine_blt_vs, kEmbeddedGL_affine_blt_vs_size };
        affineFs   = { kEmbeddedGL_affine_blt_fs, kEmbeddedGL_affine_blt_fs_size };
    } else if (isMetal) {
        vsSprite   = { kEmbeddedMetal_vs_sprite, kEmbeddedMetal_vs_sprite_size };
        fsTexture  = { kEmbeddedMetal_fs_texture, kEmbeddedMetal_fs_texture_size };
        vsFullscreen = { kEmbeddedMetal_vs_fullscreen, kEmbeddedMetal_vs_fullscreen_size };
        fsBlend    = { kEmbeddedMetal_fs_blend, kEmbeddedMetal_fs_blend_size };
        fsTransition = { kEmbeddedMetal_fs_transition, kEmbeddedMetal_fs_transition_size };
        fsVfx      = { kEmbeddedMetal_fs_vfx, kEmbeddedMetal_fs_vfx_size };
        stretchVs  = { kEmbeddedMetal_stretch_blt_vs, kEmbeddedMetal_stretch_blt_vs_size };
        stretchFs  = { kEmbeddedMetal_stretch_blt_fs, kEmbeddedMetal_stretch_blt_fs_size };
        affineVs   = { kEmbeddedMetal_affine_blt_vs, kEmbeddedMetal_affine_blt_vs_size };
        affineFs   = { kEmbeddedMetal_affine_blt_fs, kEmbeddedMetal_affine_blt_fs_size };
    }

    // Stretch/affine fall back to sprite+texture on platforms without
    // dedicated blit bytecode (Vulkan today): no src-rect UV mapping.
    if (stretchVs.size == 0) { stretchVs = vsSprite; stretchFs = fsTexture; }
    if (affineVs.size == 0)  { affineVs  = vsSprite; affineFs  = fsTexture; }

    if (!vsSprite.data || vsSprite.size == 0 ||
        !fsTexture.data || fsTexture.size == 0) {
        printf("[BgfxShaderManager] No embedded shaders for %s. "
               "Debug text only.\n", bgfx::getRendererName(renderer));
        return;
    }

    auto buildProgram = [&](const Bytecode& vs, const Bytecode& fs,
                            const char* name,
                            const ShaderUniformMetadata* fragmentUniform) -> bgfx::ProgramHandle {
        if (!vs.data || !fs.data || vs.size == 0 || fs.size == 0) {
            printf("[BgfxShaderManager] %s: no embedded data.\n", name);
            return BGFX_INVALID_HANDLE;
        }
        const uint16_t vsAttrs[] = { 0x0001, 0x0010 };
        bgfx::ShaderHandle vsHandle = buildBgfxShader(
            vs.data, (uint32_t)vs.size, false, 2, vsAttrs);
        bgfx::ShaderHandle fsHandle = buildBgfxShader(
            fs.data, (uint32_t)fs.size, true, 0, nullptr, fragmentUniform);
        if (!bgfx::isValid(vsHandle) || !bgfx::isValid(fsHandle)) {
            printf("[BgfxShaderManager] %s: shader build failed.\n", name);
            if (bgfx::isValid(vsHandle)) bgfx::destroy(vsHandle);
            if (bgfx::isValid(fsHandle)) bgfx::destroy(fsHandle);
            return BGFX_INVALID_HANDLE;
        }
        bgfx::ProgramHandle prog = bgfx::createProgram(vsHandle, fsHandle, true);
        printf("[BgfxShaderManager] %s program %s.\n", name,
               bgfx::isValid(prog) ? "READY" : "FAILED");
        return prog;
    };

    constexpr uint8_t kUniformFragmentBit = 0x10;
    const ShaderUniformMetadata vfxParams = {
        "VFXParams",
        uint8_t(bgfx::UniformType::Vec4) | kUniformFragmentBit,
        3, 0, 3, 48
    };

    m_fallbackProgram = buildProgram(vsSprite, fsTexture, "Fallback", nullptr);
    m_blendProgram = buildProgram(vsFullscreen, fsBlend, "Blend", nullptr);
    m_transitionProgram = buildProgram(vsFullscreen, fsTransition, "Transition", nullptr);
    m_vfxProgram = buildProgram(vsFullscreen, fsVfx, "VFX", &vfxParams);
    m_stretchProgram = buildProgram(stretchVs, stretchFs, "StretchBlt", nullptr);
    m_affineProgram = buildProgram(affineVs, affineFs, "AffineBlt", nullptr);

    // Verify fallback program is valid before registering
    if (!bgfx::isValid(m_fallbackProgram)) {
        fprintf(stderr, "[BgfxShaderManager] FALLBACK PROGRAM INVALID, all rendering disabled!\n");
    }

    // -- Create uniform handles for effect cbuffers -------------------
    m_u_blendParams = bgfx::createUniform("BlendParams",  bgfx::UniformType::Vec4, 2);
    m_u_transParams = bgfx::createUniform("TransParams",  bgfx::UniformType::Vec4, 1);
    m_u_vfxParams   = bgfx::createUniform("VFXParams",    bgfx::UniformType::Vec4, 3);
    m_u_stretchParams = bgfx::createUniform("StretchParams", bgfx::UniformType::Vec4, 1);
    m_u_affineParams  = bgfx::createUniform("AffineParams",  bgfx::UniformType::Vec4, 4);

    // -- Register with ShaderCache -------------------------------------
    if (bgfx::isValid(m_blendProgram)) {
        static const int kAlphaModes[] = {
            (int)BlendMode::Normal,    (int)BlendMode::Multiply,
            (int)BlendMode::Screen,    (int)BlendMode::Overlay,
            (int)BlendMode::Darken,    (int)BlendMode::Lighten,
            (int)BlendMode::ColorDodge,(int)BlendMode::ColorBurn,
            (int)BlendMode::HardLight, (int)BlendMode::SoftLight
        };
        for (int mode : kAlphaModes) {
            CompositeShaderKey key = { (uint8_t)mode };
            CompositeShaderCache::instance().registerProgram(key, m_blendProgram);
        }
        printf("[BgfxShaderManager] Registered 10 blend modes with ShaderCache.\n");
    }
    CompositeShaderCache::instance().precompileCommon();
    if (bgfx::isValid(m_fallbackProgram)) {
        CompositeShaderKey fk;
        CompositeShaderCache::instance().registerProgram(fk, m_fallbackProgram);
    }
}
} // namespace Caesura
