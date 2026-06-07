// Caesura (AmeKAG) - Affine Blit Fragment Shader (HLSL ps_4_0)
Texture2D    s_texColor : register(t0);
SamplerState s_texSamp  : register(s0);
struct PSInput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};
float4 main(PSInput input) : SV_TARGET {
    return s_texColor.Sample(s_texSamp, input.texcoord);
}