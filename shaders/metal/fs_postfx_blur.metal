// Caesura (AmeKAG) - Soft Gaussian Blur FS (Metal reference)
#include <metal_stdlib>
using namespace metal;
struct PIn { float4 pos [[position]]; float2 uv; };
struct PostFxParams { float4 p0; float4 p1; float4 p2; float4 p3; };
fragment float4 fs_postfx_blur(PIn in [[stage_in]],
    texture2d<float> tex [[texture(0)]],
    sampler smp [[sampler(0)]],
    constant PostFxParams& u [[buffer(0)]]) {
    float2 uv = in.uv;
    float2 texel = u.p2.xy;
    float2 off = texel * max(u.p0.y, 0.0);
    const float w[3] = {0.227027, 0.1945946, 0.1216216};
    float4 sum = tex.sample(smp, uv) * w[1];
    for (int x = 1; x <= 2; ++x) {
        float f = w[x];
        sum += tex.sample(smp, uv + float2(off.x * float(x), 0.0)) * f;
        sum += tex.sample(smp, uv - float2(off.x * float(x), 0.0)) * f;
        sum += tex.sample(smp, uv + float2(0.0, off.y * float(x))) * f;
        sum += tex.sample(smp, uv - float2(0.0, off.y * float(x))) * f;
    }
    float4 original = tex.sample(smp, uv);
    float amount = clamp(u.p0.z, 0.0, 1.0);
    float4 mixed = mix(original, sum, amount);
    mixed.rgb *= max(u.p1.rgb, float3(0.0));
    return float4(mixed.rgb, original.a);
}
