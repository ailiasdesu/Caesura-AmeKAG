// Caesura (AmeKAG) - 3D LUT color-grade PS (HLSL ps_4_0, t214)
// 2D-packed 3D LUT sampling: N slices of N x N laid out horizontally
// (256x16 -> 16^3, 4096x64 -> 64^3). uv maps to a cube entry; the result
// is lerped with the source by intensity (u_p0.x).
Texture2D    s_tex  : register(t0);
Texture2D    s_lut  : register(t1);
SamplerState s_samp : register(s0);
cbuffer PostFxParams : register(b0) {
    float4 u_p0; // x=intensity(0..1)
    float4 u_p1; // unused
    float4 u_p2; // (1/w, 1/h, lutSize N, 0)
    float4 u_p3; // spare
};
struct PSInput { float4 p : SV_POSITION; float2 t : TEXCOORD0; };

float4 main(PSInput i) : SV_TARGET {
    float2 uv   = i.t;
    float4 src  = s_tex.Sample(s_samp, uv);
    float  N    = max(floor(u_p2.z + 0.5), 2.0);
    float  col  = uv.x * N * N;
    float  slice = floor(col / N);
    float  sx   = col - slice * N + 0.5;
    float  sy   = uv.y * N + 0.5;
    float2 luv  = float2(sx / (N * N), sy / N);
    float4 lut  = s_lut.Sample(s_samp, luv);
    float3 outC = lerp(src.rgb, lut.rgb, clamp(u_p0.x, 0.0, 1.0));
    return float4(outC, src.a);
}
