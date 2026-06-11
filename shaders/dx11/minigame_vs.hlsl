// Caesura (AmeKAG) - MiniGame 3D Vertex Shader (HLSL vs_4_0)
// Direct3D 11 HLSL for bgfx embedding
// Input:  position + normal
// Output: world-space position + normal for fragment lighting

struct VSInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
};

uniform float4x4 u_mtx;       // model matrix (bgfx setTransform)
uniform float4x4 u_viewProj;  // view-projection (bgfx setViewTransform)

VSOutput main(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(float4(input.position, 1.0), u_mtx);
    output.worldPos = worldPos.xyz;
    output.normal   = normalize(mul(float4(input.normal, 0.0), u_mtx).xyz);
    output.position = mul(worldPos, u_viewProj);
    return output;
}
