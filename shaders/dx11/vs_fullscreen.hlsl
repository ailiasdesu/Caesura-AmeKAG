// Caesura (AmeKAG) - Fullscreen Quad VS (HLSL vs_4_0)
struct VSOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};
VSOutput main(uint vertexID : SV_VertexID) {
    VSOutput o;
    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    o.position = float4(uv * float2(2,-2) + float2(-1,1), 0, 1);
    o.texcoord = uv * float2(0.5,-0.5) + float2(0,1);
    return o;
}