// Caesura (AmeKAG) - Stretch Blit Fragment Shader (Metal)
// Corresponds to spec [2.4] stretch_blt.fs

#include <metal_stdlib>
using namespace metal;

struct PSInput {
    float4 position [[position]];
    float2 texcoord;
};

struct StretchParams {
    float4 u_stretchParams;    // xy=src_offset, zw=src_scale
};

fragment float4 stretch_blt_fs(PSInput in [[stage_in]],
                               texture2d<float> s_texColor [[texture(0)]],
                               sampler s_samp [[sampler(0)]],
                               constant StretchParams& params [[buffer(0)]]) {
    float2 uv = params.u_stretchParams.xy + in.texcoord * params.u_stretchParams.zw;
    return s_texColor.sample(s_samp, uv);
}
