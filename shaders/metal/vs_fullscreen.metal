// Caesura (AmeKAG) - Fullscreen Quad Vertex Shader (Metal)
// Procedural quad from vertex ID (no vertex buffer needed)
// Corresponds to shaders/dx11/vs_fullscreen.hlsl

#include <metal_stdlib>
using namespace metal;

struct VSOutput {
    float4 position [[position]];
    float2 texcoord;
};

vertex VSOutput vs_fullscreen(uint vertexID [[vertex_id]]) {
    VSOutput out;
    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    out.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    out.texcoord = uv * float2(0.5, -0.5) + float2(0.0, 1.0);
    return out;
}
