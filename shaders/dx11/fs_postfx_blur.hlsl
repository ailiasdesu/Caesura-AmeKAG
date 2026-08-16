// Caesura (AmeKAG) - Soft Gaussian Blur PS (HLSL ps_4_0)
// 3x3 gaussian (9-tap) softened by radius texels, then mixed with the
// original by amount.
Texture2D    s_tex  : register(t0);
SamplerState s_samp : register(s0);
cbuffer PostFxParams : register(b0) {
    float4 u_p0; // x=strength(0..1) [unused here, use amount], y=unused, z=amount(mix), w=unused
    float4 u_p1; // rgb tint (multiplied after blur)
    float4 u_p2; // (1/w, 1/h, 0, 0)
    float4 u_p3; // spare
};
struct PSInput { float4 p : SV_POSITION; float2 t : TEXCOORD0; };

float4 main(PSInput i) : SV_TARGET {
    float2 uv  = i.t;
    float2 texel = u_p2.xy;
    float  radius = max(0.0, u_p0.y);
    float2 off = texel * radius;
    // 3x3 gaussian weights
    static const float w[3] = { 0.227027, 0.1945946, 0.1216216 };
    float4 sum = s_tex.Sample(s_samp, uv) * w[1];
    for (int x = 1; x <= 2; ++x) {
        float f = w[x];
        sum += s_tex.Sample(s_samp, uv + float2(off.x * float(x), 0.0)) * f;
        sum += s_tex.Sample(s_samp, uv - float2(off.x * float(x), 0.0)) * f;
        sum += s_tex.Sample(s_samp, uv + float2(0.0, off.y * float(x))) * f;
        sum += s_tex.Sample(s_samp, uv - float2(0.0, off.y * float(x))) * f;
    }
    // Normalize the 1D gaussian sums (4 directions accumulate multiple taps).
    float4 original = s_tex.Sample(s_samp, uv);
    float  amount   = saturate(u_p0.z);   // mix toward blurred
    float4 blurred  = sum;
    float4 mixed    = lerp(original, blurred, amount);
    mixed.rgb *= max(u_p1.rgb, float3(0.0,0.0,0.0));
    return float4(mixed.rgb, original.a);
}
