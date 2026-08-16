// Caesura (AmeKAG) - Vignette PS (HLSL ps_4_0)
// Full-screen radial darkening toward the corners, tint optional.
Texture2D    s_tex  : register(t0);
SamplerState s_samp : register(s0);
cbuffer PostFxParams : register(b0) {
    float4 u_p0; // x=strength(0..1), y=inner radius(0..~1.2), z=unused, w=unused
    float4 u_p1; // rgb tint
    float4 u_p2; // (1/w, 1/h, 0, 0)
    float4 u_p3; // spare
};
struct PSInput { float4 p : SV_POSITION; float2 t : TEXCOORD0; };

float4 main(PSInput i) : SV_TARGET {
    float2 uv  = i.t;
    float3 col = s_tex.Sample(s_samp, uv).rgb;
    float2 cc  = uv - 0.5;
    float r    = length(cc) * 2.0;          // 0 center -> ~1.414 corner
    float inner = max(u_p0.y, 0.0001);
    float edge  = 1.7;
    float v     = saturate((r - inner) / (edge - inner)); // 0 inside -> 1 at corners
    float amount = u_p0.x;                   // strength 0..1
    float3 tint = max(u_p1.rgb, float3(0.0,0.0,0.0));
    float3 base = col;
    // Darken toward tint color proportional to vignette amount.
    float3 vign  = lerp(base, base * tint, v);
    float3 outC  = lerp(base, vign, amount);
    return float4(outC, s_tex.Sample(s_samp, uv).a);
}
