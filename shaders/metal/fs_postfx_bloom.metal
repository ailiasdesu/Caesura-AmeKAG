// Caesura (AmeKAG) - Bloom composite FS (Metal reference)
#include <metal_stdlib>
using namespace metal;
struct PIn { float4 pos [[position]]; float2 uv; };
struct PostFxParams { float4 p0; float4 p1; float4 p2; float4 p3; };
fragment float4 fs_postfx_bloom(PIn in [[stage_in]],
    texture2d<float> tex [[texture(0)]],
    texture2d<float> bloom [[texture(1)]],
    sampler smp [[sampler(0)]],
    constant PostFxParams& u [[buffer(0)]]) {
    float4 src = tex.sample(smp, in.uv);
    float thr = max(u.p0.z, 0.0);
    float lum = dot(src.rgb, float3(0.2126,0.7152,0.0722));
    float3 bright = max(lum - thr, 0.0) * src.rgb;
    float3 bl = bloom.sample(smp, in.uv).rgb;
    float3 add = (bl + bright) * max(u.p0.x, 0.0) * max(u.p1.rgb, float3(0.0));
    return float4(clamp(src.rgb + add, 0.0, 1.0), src.a);
}
