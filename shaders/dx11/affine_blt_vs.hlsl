// Caesura (AmeKAG) - Affine Blit Vertex Shader (HLSL vs_4_0)
cbuffer AffineParams : register(b0) {
    float4 u_row0;
    float4 u_row1;
    float4 u_srcRect;
};
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
    float2 tp;
    tp.x = u_row0.x * input.position.x + u_row0.y * input.position.y + u_row0.z;
    tp.y = u_row1.x * input.position.x + u_row1.y * input.position.y + u_row1.z;
    output.texcoord = u_srcRect.xy + tp * u_srcRect.zw;
    output.position = float4(input.position * 2.0 - 1.0, 0.0, 1.0);
    return output;
}