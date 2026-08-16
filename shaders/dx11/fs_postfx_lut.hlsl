// Caesura (AmeKAG) - LUT color-grade PS (HLSL ps_4_0)
// 3x3-diagonal-per-channel grade blended by lutMix and master strength.
// The compact grade scales each channel by the given tint (1.0 = neutral),
// then mixes toward a unit-saturation boost by lutMix.
Texture2D    s_tex  : register(t0);
SamplerState s_samp : register(s0);
cbuffer PostFxParams : register(b0) {
    float4 u_p0; // x=strength(0..1), y=unused, z=unused, w=lutMix(0..1)
    float4 u_p1; // rgb grade multiplicands
    float4 u_p2; // (1/w, 1/h, 0, 0)
    float4 u_p3; // spare
};
struct PSInput { float4 p : SV_POSITION; float2 t : TEXCOORD0; };

float4 main(PSInput i) : SV_TARGET {
    float2 uv   = i.t;
    float4 src  = s_tex.Sample(s_samp, uv);
    float3 mult = max(u_p1.rgb, float3(0.0,0.0,0.0));
    float3 graded = src.rgb * mult;                 // per-channel scale grade
    float luma    = dot(src.rgb, float3(0.2126,0.7152,0.0722));
    float3 boost  = lerp(float3(luma,luma,luma), src.rgb * mult, 0.5); // mild sat lift
    float3 mixed  = lerp(src.rgb, graded, u_p0.w);  // lutMix blends grade in
    mixed         = lerp(mixed, boost, u_p0.x * 0.5); // strength eases toward boost
    return float4(mixed, src.a);
}
