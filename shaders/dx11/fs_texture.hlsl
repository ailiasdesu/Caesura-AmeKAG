// Caesura (AmeKAG) - Default 2D Texture Fragment Shader (HLSL ps_4_0)
Texture2D s_texture : register(t0);
SamplerState s_textureSampler : register(s0);

struct PSInput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    return s_texture.Sample(s_textureSampler, input.texcoord);
}
