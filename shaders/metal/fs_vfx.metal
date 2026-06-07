// Caesura (AmeKAG) - VFX Fragment Shader (Metal)
// fade / blur / quake post-processing
// Corresponds to shaders/dx11/fs_vfx.hlsl

#include <metal_stdlib>
using namespace metal;

struct PSInput {
    float4 position [[position]];
    float2 texcoord;
};

struct VFXParams {
    float4 u_color;        // rgb + fadeAlpha
    float2 u_blurR;
    float  u_qx;
    float  u_qy;
    int    u_effect;
    float3 u_p;
};

fragment float4 xlatMtlMain(PSInput in [[stage_in]],
                       texture2d<float> s_tex [[texture(0)]],
                       sampler s_samp [[sampler(0)]],
                       constant VFXParams& params [[buffer(0)]]) {
    float2 uv = in.texcoord;
    float4 c = float4(0.0);

    if (params.u_effect == 1) {
        // Fade
        c = mix(s_tex.sample(s_samp, uv), params.u_color, params.u_color.a);
    } else if (params.u_effect == 2) {
        // 3x3 box blur
        float2 stepSize = params.u_blurR * 0.3333f;
        for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
                c += s_tex.sample(s_samp, uv + float2(float(x), float(y)) * stepSize);
            }
        }
        c /= 9.0f;
    } else if (params.u_effect == 3) {
        // Quake
        c = s_tex.sample(s_samp, uv + float2(params.u_qx, params.u_qy));
    } else {
        // Pass-through
        c = s_tex.sample(s_samp, uv);
    }

    return c;
}
