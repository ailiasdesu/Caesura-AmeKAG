// Caesura (AmeKAG) - Bloom composite PS (HLSL ps_4_0)
// Blends the full-resolution scene with a pre-extracted/blurred bloom texture.
// Used for the FINAL bloom composite pass (bright-pass + downsample + blur
// are separate passes driven by the host; this shader does extract + add).
Texture2D    s_tex  : register(t0);  // full-res scene
Texture2D    s_bloom : register(t1); // blurred bright-pass bloom
SamplerState s_samp : register(s0);
SamplerState s_samp1 : register(s1);
cbuffer PostFxParams : register(b0) {
    float4 u_p0; // x=strength(0..1), y=unused, z=amount(threshold), w=unused
    float4 u_p1; // rgb tint
    float4 u_p2; // (1/w,1/h,0,0)
    float4 u_p3; // spare
};
struct PSInput { float4 p : SV_POSITION; float2 t : TEXCOORD0; };

float4 main(PSInput i) : SV_TARGET {
    float2 uv   = i.t;
    float4 src  = s_tex.Sample(s_samp, uv);
    float  thr  = max(u_p0.z, 0.0);   // bright-pass threshold
    float  lum  = dot(src.rgb, float3(0.2126,0.7152,0.0722));
    // In the composite pass we also re-extract (handles the single-texture
    // fallback where the host skips the separate extract pass).
    float3 bright = max(lum - thr, 0.0) * src.rgb;
    float3 bloom  = s_bloom.Sample(s_samp1, uv).rgb; // from blur pass when present
    // sampler1 falls back to sampler0 (same texture) if host bound it so.
    float3 add    = (bloom + bright) * max(u_p0.x, 0.0);
    add.rgb *= max(u_p1.rgb, float3(0.0,0.0,0.0));
    return float4(saturate(src.rgb + add), src.a);
}
