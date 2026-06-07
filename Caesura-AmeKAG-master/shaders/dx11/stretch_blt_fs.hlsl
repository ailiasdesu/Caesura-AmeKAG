// Caesura (AmeKAG) - Stretch Blit Fragment Shader (HLSL ps_4_0)
Texture2D    s_texColor : register(t0);
SamplerState s_texSamp  : register(s0);
cbuffer StretchParams : register(b0) {
    float4 u_stretchParams;
};
struct PSInput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};
float4 main(PSInput input) : SV_TARGET {
    float2 uv = u_stretchParams.xy + input.texcoord * u_stretchParams.zw;
    return s_texColor.Sample(s_texSamp, uv);
}