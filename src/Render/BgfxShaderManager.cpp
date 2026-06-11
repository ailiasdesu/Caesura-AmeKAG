#include "BgfxShaderManager.h"
#include "EmbeddedShaders.h"
#include "ShaderCache.h"
#include <bx/bx.h>
#include <bx/readerwriter.h>
#include <bx/error.h>
#include <cstdio>

namespace Caesura {

BgfxShaderManager::~BgfxShaderManager() {
    if (bgfx::isValid(m_fallbackProgram))    bgfx::destroy(m_fallbackProgram);
    if (bgfx::isValid(m_blendProgram))       bgfx::destroy(m_blendProgram);
    if (bgfx::isValid(m_transitionProgram))  bgfx::destroy(m_transitionProgram);
    if (bgfx::isValid(m_vfxProgram))         bgfx::destroy(m_vfxProgram);
    if (bgfx::isValid(m_stretchProgram))     bgfx::destroy(m_stretchProgram);
    if (bgfx::isValid(m_affineProgram))      bgfx::destroy(m_affineProgram);
    if (bgfx::isValid(m_texSampler))         bgfx::destroy(m_texSampler);
    if (bgfx::isValid(m_u_blendParams))      bgfx::destroy(m_u_blendParams);
    if (bgfx::isValid(m_u_transParams))      bgfx::destroy(m_u_transParams);
    if (bgfx::isValid(m_u_vfxParams))        bgfx::destroy(m_u_vfxParams);
    if (bgfx::isValid(m_u_stretchParams))    bgfx::destroy(m_u_stretchParams);
    if (bgfx::isValid(m_u_affineParams))     bgfx::destroy(m_u_affineParams);
}

static bgfx::ShaderHandle buildBgfxShader(const uint8_t* bytecode, uint32_t codeSize,
                                            bool fragment, uint8_t numAttrs, const uint16_t* attrIds) {
    // [10.2.3] Shader safety: reject bytecode > 64 KB (SPIR-V/DXBC limit)
    if (codeSize > 65536) {
        fprintf(stderr, "[BgfxShaderManager] Shader rejected: %u bytes exceeds 64 KB limit.\n", codeSize);
        return BGFX_INVALID_HANDLE;
    }

    const uint16_t uniformCount = 0;

    const uint32_t totalSize = 4 + 4 + 4 + 2
                             + 4 + codeSize + 1
                             + 1 + 2 * numAttrs
                             + 2;

    const bgfx::Memory* mem = bgfx::alloc(totalSize);
    bx::StaticMemoryBlockWriter writer(mem->data, mem->size);
    bx::ErrorAssert err;

    const uint32_t magic = fragment
        ? BX_MAKEFOURCC('F', 'S', 'H', 11)
        : BX_MAKEFOURCC('V', 'S', 'H', 11);
    bx::write(&writer, magic, err);
    bx::write(&writer, uint32_t(0), err);
    bx::write(&writer, uint32_t(0), err);
    bx::write(&writer, uniformCount, err);

    bx::write(&writer, codeSize, err);
    for (uint32_t i = 0; i < codeSize; ++i)
        bx::write(&writer, bytecode[i], err);
    bx::write(&writer, uint8_t(0), err);

    bx::write(&writer, numAttrs, err);
    for (uint8_t i = 0; i < numAttrs; ++i)
        bx::write(&writer, attrIds[i], err);

    bx::write(&writer, uint16_t(0), err);

    return bgfx::createShader(mem);
}


// initEmbeddedShaders
// Picks the correct embedded bytecode (SPIR-V for Vulkan, DXBC for
// D3D11/D3D12), wraps it in a proper bgfx binary header via
// buildBgfxShader(), and registers the resulting program as the
// engine-wide fallback for 2-D quad rendering and RTT blits.

void BgfxShaderManager::initEmbeddedShaders() {
    const bgfx::RendererType::Enum renderer = bgfx::getCaps()->rendererType;

    printf("[BgfxShaderManager] initEmbeddedShaders: renderer=%s\n",
           bgfx::getRendererName(renderer));
    // Initialize ShaderCache before registering any programs
    CompositeShaderCache::instance().init();

    const uint8_t*  vsCode     = nullptr;
    uint32_t        vsCodeSize = 0;
    const uint8_t*  fsCode     = nullptr;
    uint32_t        fsCodeSize = 0;

    // bgfx RendererType: 2=Direct3D11, 3=Direct3D12, 9=Vulkan
    if (renderer == bgfx::RendererType::Vulkan) {
        if (kEmbeddedVS_SpriteSize > 0 && kEmbeddedFS_TextureSize > 0) {
            vsCode     = reinterpret_cast<const uint8_t*>(kEmbeddedVS_Sprite);
            vsCodeSize = uint32_t(kEmbeddedVS_SpriteSize * sizeof(uint32_t));
            fsCode     = reinterpret_cast<const uint8_t*>(kEmbeddedFS_Texture);
            fsCodeSize = uint32_t(kEmbeddedFS_TextureSize * sizeof(uint32_t));
        }

    } else if (renderer == bgfx::RendererType::Direct3D11 ||
               renderer == bgfx::RendererType::Direct3D12) {
        if (kEmbeddedDXBC_VS_Sprite_size > 0 && kEmbeddedDXBC_FS_Texture_size > 0) {
            vsCode     = kEmbeddedDXBC_VS_Sprite;
            vsCodeSize = uint32_t(kEmbeddedDXBC_VS_Sprite_size);
            fsCode     = kEmbeddedDXBC_FS_Texture;
            fsCodeSize = uint32_t(kEmbeddedDXBC_FS_Texture_size);
        }
    }

    if (!vsCode || !fsCode) {
        printf("[BgfxShaderManager] No embedded shaders for %s. "
               "Debug text only.\n", bgfx::getRendererName(renderer));
        return;
    }

    // Vertex shader input: Position (0x0001) + TexCoord0 (0x0010)
    const uint16_t vsAttrs[] = { 0x0001, 0x0010 };

    bgfx::ShaderHandle vs = buildBgfxShader(vsCode, vsCodeSize, false, 2, vsAttrs);
    bgfx::ShaderHandle fs = buildBgfxShader(fsCode, fsCodeSize, true,  0, nullptr);

    if (!bgfx::isValid(vs) || !bgfx::isValid(fs)) {
        fprintf(stderr, "[BgfxShaderManager] buildBgfxShader failed.\n");
        if (bgfx::isValid(vs)) bgfx::destroy(vs);
        if (bgfx::isValid(fs)) bgfx::destroy(fs);
        return;
    }

    printf("[BgfxShaderManager] Shader binaries built (VS=%u B, FS=%u B).\n",
           vsCodeSize, fsCodeSize);

    m_fallbackProgram = bgfx::createProgram(vs, fs, true);

    if (!bgfx::isValid(m_fallbackProgram)) {
        fprintf(stderr, "[BgfxShaderManager] createProgram rejected shaders. "
                "Debug text only.\n");
    } else {
        
    // -- Create effect shader programs from embedded DXBC -------------
    auto createProgramFromDXBC = [&](const uint8_t* vsData, size_t vsSize,
                                      const uint8_t* fsData, size_t fsSize,
                                      const char* name) -> bgfx::ProgramHandle {
        if (!vsData || !fsData || vsSize == 0 || fsSize == 0) {
            printf("[BgfxShaderManager] %s: no embedded data.\n", name);
            return BGFX_INVALID_HANDLE;
        }
        const uint16_t vsAttrs[] = { 0x0001, 0x0010 };
        bgfx::ShaderHandle vs = buildBgfxShader(vsData, (uint32_t)vsSize, false, 2, vsAttrs);
        bgfx::ShaderHandle fs = buildBgfxShader(fsData, (uint32_t)fsSize, true, 0, nullptr);
        if (!bgfx::isValid(vs) || !bgfx::isValid(fs)) {
            printf("[BgfxShaderManager] %s: shader build failed.\n", name);
            if (bgfx::isValid(vs)) bgfx::destroy(vs);
            if (bgfx::isValid(fs)) bgfx::destroy(fs);
            return BGFX_INVALID_HANDLE;
        }
        bgfx::ProgramHandle prog = bgfx::createProgram(vs, fs, true);
        printf("[BgfxShaderManager] %s program %s.\n", name,
               bgfx::isValid(prog) ? "READY" : "FAILED");
        return prog;
    };

    if (renderer == bgfx::RendererType::Direct3D11 ||
        renderer == bgfx::RendererType::Direct3D12) {

        m_blendProgram = createProgramFromDXBC(
            kEmbeddedDXBC_vs_fullscreen, kEmbeddedDXBC_vs_fullscreen_size,
            kEmbeddedDXBC_fs_blend, kEmbeddedDXBC_fs_blend_size, "Blend");

        m_transitionProgram = createProgramFromDXBC(
            kEmbeddedDXBC_vs_fullscreen, kEmbeddedDXBC_vs_fullscreen_size,
            kEmbeddedDXBC_fs_transition, kEmbeddedDXBC_fs_transition_size, "Transition");

        m_vfxProgram = createProgramFromDXBC(
            kEmbeddedDXBC_vs_fullscreen, kEmbeddedDXBC_vs_fullscreen_size,
            kEmbeddedDXBC_fs_vfx, kEmbeddedDXBC_fs_vfx_size, "VFX");
    }
        // Verify fallback program is valid before registering
    if (!bgfx::isValid(m_fallbackProgram)) {
        fprintf(stderr, "[BgfxShaderManager] FALLBACK PROGRAM INVALID, all rendering disabled!\n");
    }

    m_stretchProgram = createProgramFromDXBC(
            kEmbeddedDXBC_stretch_blt_vs, kEmbeddedDXBC_stretch_blt_vs_size,
            kEmbeddedDXBC_stretch_blt_fs, kEmbeddedDXBC_stretch_blt_fs_size, "StretchBlt");

        m_affineProgram = createProgramFromDXBC(
            kEmbeddedDXBC_affine_blt_vs, kEmbeddedDXBC_affine_blt_vs_size,
            kEmbeddedDXBC_affine_blt_fs, kEmbeddedDXBC_affine_blt_fs_size, "AffineBlt");

    // -- Create uniform handles for effect cbuffers -------------------
    m_u_blendParams = bgfx::createUniform("BlendParams",  bgfx::UniformType::Vec4, 2);
    m_u_transParams = bgfx::createUniform("TransParams",  bgfx::UniformType::Vec4, 1);
    m_u_vfxParams   = bgfx::createUniform("VFXParams",    bgfx::UniformType::Vec4, 3);

    // -- Stretch / Affine uniforms ----------------------------------
    m_u_stretchParams = bgfx::createUniform("StretchParams", bgfx::UniformType::Vec4, 1);
    m_u_affineParams  = bgfx::createUniform("AffineParams",  bgfx::UniformType::Vec4, 4);
} // namespace Caesura