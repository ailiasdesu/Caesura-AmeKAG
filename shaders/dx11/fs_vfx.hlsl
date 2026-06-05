// Caesura (AmeKAG) - VFX PS (HLSL ps_4_0)
Texture2D    s_tex  : register(t0);
SamplerState s_samp : register(s0);
cbuffer VFXParams : register(b0) {
    float4 u_color; float2 u_blurR; float u_qx; float u_qy; int u_effect; float3 u_p;
};
struct PSInput { float4 p : SV_POSITION; float2 t : TEXCOORD0; };

float4 main(PSInput i) : SV_TARGET {
    float2 uv = i.t;
    float4 c = 0;
    if (u_effect == 1) {
        c = lerp(s_tex.Sample(s_samp, uv), u_color, u_color.a);
    } else if (u_effect == 2) {
        float2 st = u_blurR * 0.3333;
        for (int y=-1; y<=1; y++) for (int x=-1; x<=1; x++)
            c += s_tex.Sample(s_samp, uv + float2(x,y)*st);
        c /= 9.0;
    } else if (u_effect == 3) {
        c = s_tex.Sample(s_samp, uv + float2(u_qx, u_qy));
    } else {
        c = s_tex.Sample(s_samp, uv);
    }
    return c;
}