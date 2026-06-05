// Caesura (AmeKAG) - Stretch Blit Vertex Shader (HLSL vs_4_0)
struct VSInput {
    float2 position : POSITION;
    float2 texcoord : TEXCOORD0;
};
struct VSOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};
VSOutput main(VSInput input) {
    VSOutput output;
    output.position = float4(input.position * 2.0 - 1.0, 0.0, 1.0);
    output.texcoord = input.texcoord;
    return output;
}