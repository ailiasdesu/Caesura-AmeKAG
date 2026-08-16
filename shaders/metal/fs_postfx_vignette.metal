// Caesura (AmeKAG) - Vignette FS (Metal reference)
#include <metal_stdlib>
using namespace metal;
struct PIn { float4 pos [[position]]; float2 uv; };
struct PostFxParams { float4 p0; float4 p1; float4 p2; float4 p3; };
fragment float4 fs_postfx_vignette(PIn in [[stage_in]],
    texture2d<float> tex [[texture(0)]],
    sampler smp [[sampler(0)]],
    constant PostFxParams& u [[buffer(0)]]) {
    float4 src = tex.sample(smp, in.uv);
    float2 cc = in.uv - 0.5;
    float r = length(cc) * 2.0;
    float inner = max(u.p0.y, 0.0001);
    float v = clamp((r - inner) / (1.7 - inner), 0.0, 1.0);
    float3 tint = max(u.p1.rgb, float3(0.0));
    float3 vign = mix(src.rgb, src.rgb * tint, v);
    float3 outC = mix(src.rgb, vign, u.p0.x);
    return float4(outC, src.a);
}
