// Caesura (AmeKAG) - 3D LUT color-grade FS (Metal reference, t214)
#include <metal_stdlib>
using namespace metal;
struct PIn { float4 pos [[position]]; float2 uv; };
struct PostFxParams { float4 p0; float4 p1; float4 p2; float4 p3; };
fragment float4 fs_postfx_lut3d(PIn in [[stage_in]],
    texture2d<float> tex [[texture(0)]],
    texture2d<float> lutTex [[texture(1)]],
    sampler smp [[sampler(0)]],
    constant PostFxParams& u [[buffer(0)]]) {
    float4 src = tex.sample(smp, in.uv);
    float N = max(floor(u.p2.z + 0.5), 2.0);
    float col = in.uv.x * N * N;
    float slice = floor(col / N);
    float sx = col - slice * N + 0.5;
    float sy = in.uv.y * N + 0.5;
    float2 luv = float2(sx / (N * N), sy / N);
    float4 lut = lutTex.sample(smp, luv);
    float3 outC = mix(src.rgb, lut.rgb, clamp(u.p0.x, 0.0, 1.0));
    return float4(outC, src.a);
}
