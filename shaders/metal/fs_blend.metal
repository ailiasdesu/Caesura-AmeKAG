// Caesura (AmeKAG) - Multi-mode Blend Fragment Shader (Metal)
// Corresponds to shaders/dx11/fs_blend.hlsl

#include <metal_stdlib>
using namespace metal;

struct PSInput {
    float4 position [[position]];
    float2 texcoord;
};

struct BlendParams {
    float4 u_opacity;       // x=baseAlpha, y=blendAlpha, z=globalAlpha, w=mode
};

float3 bAdd(float3 a, float3 b) { return a + b; }
float3 bSub(float3 a, float3 b) { return a - b; }
float3 bMul(float3 a, float3 b) { return a * b; }
float3 bScr(float3 a, float3 b) { return float3(1.0) - (float3(1.0) - a) * (float3(1.0) - b); }
float3 bOvl(float3 a, float3 b) {
    float3 r;
    r.r = a.r < 0.5 ? 2.0 * a.r * b.r : 1.0 - 2.0 * (1.0 - a.r) * (1.0 - b.r);
    r.g = a.g < 0.5 ? 2.0 * a.g * b.g : 1.0 - 2.0 * (1.0 - a.g) * (1.0 - b.g);
    r.b = a.b < 0.5 ? 2.0 * a.b * b.b : 1.0 - 2.0 * (1.0 - a.b) * (1.0 - b.b);
    return r;
}
float3 bDrk(float3 a, float3 b) { return min(a, b); }
float3 bLit(float3 a, float3 b) { return max(a, b); }
float3 bDif(float3 a, float3 b) { return abs(a - b); }

fragment float4 fs_blend(PSInput in [[stage_in]],
                         texture2d<float> s_baseTex  [[texture(0)]],
                         texture2d<float> s_blendTex [[texture(1)]],
                         sampler s_baseSamp  [[sampler(0)]],
                         sampler s_blendSamp [[sampler(1)]],
                         constant BlendParams& params [[buffer(0)]]) {
    float4 base  = s_baseTex.sample(s_baseSamp, in.texcoord) * params.u_opacity.x;
    float4 blend = s_blendTex.sample(s_blendSamp, in.texcoord) * params.u_opacity.y;
    float3 c;
    int mode = int(params.u_opacity.w);

    switch (mode) {
        case 1:  c = bAdd(base.rgb, blend.rgb); break;
        case 2:  c = bSub(base.rgb, blend.rgb); break;
        case 3:  c = bMul(base.rgb, blend.rgb); break;
        case 4:  c = bScr(base.rgb, blend.rgb); break;
        case 5:  c = bOvl(base.rgb, blend.rgb); break;
        case 6:  c = bDrk(base.rgb, blend.rgb); break;
        case 7:  c = bLit(base.rgb, blend.rgb); break;
        case 8:  c = bDif(base.rgb, blend.rgb); break;
        default: c = mix(base.rgb, blend.rgb, blend.a); break;
    }

    float alpha = clamp(base.a + blend.a, 0.0f, 1.0f);
    return float4(c, alpha) * params.u_opacity.z;
}
