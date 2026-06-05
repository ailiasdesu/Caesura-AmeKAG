// Caesura (AmeKAG) - Affine Blit Fragment Shader (Metal)
// Passthrough texture lookup (same logic as stretch_blt_fs)

#include <metal_stdlib>
using namespace metal;

struct PSInput {
    float4 position [[position]];
    float2 texcoord;
};

fragment float4 affine_blt_fs(PSInput in [[stage_in]],
                              texture2d<float> s_texColor [[texture(0)]],
                              sampler s_samp [[sampler(0)]]) {
    return s_texColor.sample(s_samp, in.texcoord);
}
