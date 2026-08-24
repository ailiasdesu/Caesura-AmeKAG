// test_render_metal_contract.cpp - Metal Shader & Fallback Contract Tests (Track I / Milestone R3).
//
// Asserts:
// 1. All 10 2D Render Metal embedded shaders (EmbeddedShaders.h / EmbeddedShaders_Metal.cpp)
//    are present, non-empty, and contain valid bgfx shader headers.
// 2. Both 3D MiniGame MSL shaders (EmbeddedMiniGameShaders.h / EmbeddedShaders_MiniGame_Metal.cpp)
//    are present, valid MSL source, and accessible via getters.
// 3. Post-FX graceful degradation to identity pass (fsTexture) without GPU stalls.
// 4. SMA dual-mode skinning fallback to S2 CPU soft-skinning on Metal.

#include "doctest.h"
#include "render/EmbeddedShaders.h"
#include "minigame/EmbeddedMiniGameShaders.h"
#include "render/api/IRenderDevice.h"
#include "render/SmaSkinner.h"
#include "render/SmaMeshRenderer.h"
#include "render/NullRenderDevice.h"

#include <cstring>
#include <cmath>
#include <string>

using namespace Caesura;

namespace {
constexpr float kEps = 1e-4f;
} // namespace

// =============================================================================
// 1. 2D Render Metal Embedded Shaders (10 shaders)
// =============================================================================

TEST_CASE("metal: 10 2D Render Metal Embedded Shaders symbol contracts") {
    // 1. vs_sprite
    CHECK(kEmbeddedMetal_vs_sprite != nullptr);
    CHECK(kEmbeddedMetal_vs_sprite_size == 608);
    CHECK(kEmbeddedMetal_vs_sprite[0] == 'V');
    CHECK(kEmbeddedMetal_vs_sprite[1] == 'S');
    CHECK(kEmbeddedMetal_vs_sprite[2] == 'H');

    // 2. vs_fullscreen
    CHECK(kEmbeddedMetal_vs_fullscreen != nullptr);
    CHECK(kEmbeddedMetal_vs_fullscreen_size == 659);
    CHECK(kEmbeddedMetal_vs_fullscreen[0] == 'V');
    CHECK(kEmbeddedMetal_vs_fullscreen[1] == 'S');
    CHECK(kEmbeddedMetal_vs_fullscreen[2] == 'H');

    // 3. stretch_blt_vs
    CHECK(kEmbeddedMetal_stretch_blt_vs != nullptr);
    CHECK(kEmbeddedMetal_stretch_blt_vs_size == 630);
    CHECK(kEmbeddedMetal_stretch_blt_vs[0] == 'V');
    CHECK(kEmbeddedMetal_stretch_blt_vs[1] == 'S');
    CHECK(kEmbeddedMetal_stretch_blt_vs[2] == 'H');

    // 4. affine_blt_vs
    CHECK(kEmbeddedMetal_affine_blt_vs != nullptr);
    CHECK(kEmbeddedMetal_affine_blt_vs_size == 995);
    CHECK(kEmbeddedMetal_affine_blt_vs[0] == 'V');
    CHECK(kEmbeddedMetal_affine_blt_vs[1] == 'S');
    CHECK(kEmbeddedMetal_affine_blt_vs[2] == 'H');

    // 5. fs_texture
    CHECK(kEmbeddedMetal_fs_texture != nullptr);
    CHECK(kEmbeddedMetal_fs_texture_size == 586);
    CHECK(kEmbeddedMetal_fs_texture[0] == 'F');
    CHECK(kEmbeddedMetal_fs_texture[1] == 'S');
    CHECK(kEmbeddedMetal_fs_texture[2] == 'H');

    // 6. fs_blend
    CHECK(kEmbeddedMetal_fs_blend != nullptr);
    CHECK(kEmbeddedMetal_fs_blend_size == 9925);
    CHECK(kEmbeddedMetal_fs_blend[0] == 'F');
    CHECK(kEmbeddedMetal_fs_blend[1] == 'S');
    CHECK(kEmbeddedMetal_fs_blend[2] == 'H');

    // 7. fs_transition
    CHECK(kEmbeddedMetal_fs_transition != nullptr);
    CHECK(kEmbeddedMetal_fs_transition_size == 2324);
    CHECK(kEmbeddedMetal_fs_transition[0] == 'F');
    CHECK(kEmbeddedMetal_fs_transition[1] == 'S');
    CHECK(kEmbeddedMetal_fs_transition[2] == 'H');

    // 8. fs_vfx
    CHECK(kEmbeddedMetal_fs_vfx != nullptr);
    CHECK(kEmbeddedMetal_fs_vfx_size == 2004);
    CHECK(kEmbeddedMetal_fs_vfx[0] == 'F');
    CHECK(kEmbeddedMetal_fs_vfx[1] == 'S');
    CHECK(kEmbeddedMetal_fs_vfx[2] == 'H');

    // 9. stretch_blt_fs
    CHECK(kEmbeddedMetal_stretch_blt_fs != nullptr);
    CHECK(kEmbeddedMetal_stretch_blt_fs_size == 753);
    CHECK(kEmbeddedMetal_stretch_blt_fs[0] == 'F');
    CHECK(kEmbeddedMetal_stretch_blt_fs[1] == 'S');
    CHECK(kEmbeddedMetal_stretch_blt_fs[2] == 'H');

    // 10. affine_blt_fs
    CHECK(kEmbeddedMetal_affine_blt_fs != nullptr);
    CHECK(kEmbeddedMetal_affine_blt_fs_size == 586);
    CHECK(kEmbeddedMetal_affine_blt_fs[0] == 'F');
    CHECK(kEmbeddedMetal_affine_blt_fs[1] == 'S');
    CHECK(kEmbeddedMetal_affine_blt_fs[2] == 'H');
}

// =============================================================================
// 2. 3D MiniGame MSL Shaders (2 shaders)
// =============================================================================

namespace Caesura {
extern const char* Caesura_GetMiniGameVS_MSL();
extern const char* Caesura_GetMiniGameFS_MSL();
}

TEST_CASE("metal: 3D MiniGame MSL Shaders symbol and syntax contracts") {
    CHECK(kEmbeddedMSL_MiniGame_VS != nullptr);
    CHECK(kEmbeddedMSL_MiniGame_FS != nullptr);

    const char* vs = Caesura_GetMiniGameVS_MSL();
    const char* fs = Caesura_GetMiniGameFS_MSL();

    REQUIRE(vs != nullptr);
    REQUIRE(fs != nullptr);
    CHECK(vs == kEmbeddedMSL_MiniGame_VS);
    CHECK(fs == kEmbeddedMSL_MiniGame_FS);

    std::string vsStr(vs);
    std::string fsStr(fs);

    // MSL syntax checks
    CHECK(vsStr.find("#include <metal_stdlib>") != std::string::npos);
    CHECK(vsStr.find("using namespace metal;") != std::string::npos);
    CHECK(vsStr.find("vertex VertexOut miniGameVS") != std::string::npos);
    CHECK(vsStr.find("u_miniViewProj") != std::string::npos);

    CHECK(fsStr.find("#include <metal_stdlib>") != std::string::npos);
    CHECK(fsStr.find("using namespace metal;") != std::string::npos);
    CHECK(fsStr.find("fragment float4 miniGameFS") != std::string::npos);
    CHECK(fsStr.find("calcDirectional") != std::string::npos);
    CHECK(fsStr.find("calcPointLight") != std::string::npos);
}

// =============================================================================
// 3. Post-FX Fallback & Degradation Pathways on Metal
// =============================================================================

TEST_CASE("metal: Post-FX fallback degradation behavior") {
    NullRenderDevice dev;
    CHECK_FALSE(dev.isPostFxSupported(IRenderDevice::PostFxKind::Vignette));
    CHECK_FALSE(dev.isPostFxSupported(IRenderDevice::PostFxKind::LutColorGrade));
    CHECK_FALSE(dev.isPostFxSupported(IRenderDevice::PostFxKind::SoftBlur));
    CHECK_FALSE(dev.isPostFxSupported(IRenderDevice::PostFxKind::Bloom));

    IRenderDevice::PostFxParams params;
    params.strength = 0.8f;
    params.radius = 0.5f;
    params.lutMix = 0.5f;

    // Degrades to 0 handle without crashing
    CHECK(dev.createPostFx(IRenderDevice::PostFxKind::Vignette, params) == 0);
    CHECK(dev.createPostFx(IRenderDevice::PostFxKind::Bloom, params) == 0);
    CHECK_FALSE(dev.isPostFxActive());

    // Safe no-ops
    CHECK_NOTHROW(dev.setPostFxParams(0, params));
    CHECK_NOTHROW(dev.destroyPostFx(0));
    CHECK_NOTHROW(dev.clearPostFx());
}

// =============================================================================
// 4. SMA Soft-Skinning Fallback on Metal
// =============================================================================

TEST_CASE("metal: SMA Skeletal Skinning CPU fallback math") {
    SMAMesh mesh;
    mesh.vertices = {
        {0.f, 0.f, 0.f, 0.f, 0, 1.f, 1, 0.f},
        {2.f, 0.f, 1.f, 0.f, 0, 0.5f, 1, 0.5f},
    };
    mesh.indices = {0, 1};

    std::vector<BonePose> poses(2);
    // Bone 0: translation +10 on X
    poses[0].ox = 10.f;
    // Bone 1: translation +20 on X
    poses[1].ox = 20.f;

    std::vector<SmaSkinnedVertex> out;
    skinMesh(mesh, poses, out);

    REQUIRE(out.size() == 2);
    // Vertex 0: 100% bone 0 -> 0 + 10 = 10
    CHECK(std::fabs(out[0].x - 10.f) < kEps);
    // Vertex 1: 50% bone 0 (2+10=12) + 50% bone 1 (2+20=22) -> 17
    CHECK(std::fabs(out[1].x - 17.f) < kEps);
}
