// Caesura (AmeKAG) - LUT color-grade FS (Metal reference)
#include <metal_stdlib>
using namespace metal;
struct PIn { float4 pos [[position]]; float2 uv; };
struct PostFxParams { float4 p0; float4 p1; float4 p2; float4 p3; };
fragment float4 fs_postfx_lut(PIn in [[stage_in]],
    texture2d<float> tex [[texture(0)]],
    sampler smp [[sampler(0)]],
    constant PostFxParams& u [[buffer(0)]]) {
    float4 src = tex.sample(smp, in.uv);
    float3 mult = max(u.p1.rgb, float3(0.0));
    float3 graded = src.rgb * mult;
    float luma = dot(src.rgb, float3(0.2126,0.7152,0.0722));
    float3 boost = mix(float3(luma), src.rgb * mult, 0.5);
    float3 mixed = mix(src.rgb, graded, u.p0.w);
    mixed = mix(mixed, boost, u.p0.x * 0.5);
    return float4(mixed, src.a);
}
