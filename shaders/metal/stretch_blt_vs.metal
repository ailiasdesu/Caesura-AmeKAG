// Caesura (AmeKAG) - Stretch Blit Vertex Shader (Metal)
// Corresponds to spec [2.4] stretch_blt

#include <metal_stdlib>
using namespace metal;

struct VSInput {
    float2 position [[attribute(0)]];
    float2 texcoord [[attribute(1)]];
};

struct VSOutput {
    float4 position [[position]];
    float2 texcoord;
};

vertex VSOutput xlatMtlMain(VSInput in [[stage_in]]) {
    VSOutput out;
    out.position = float4(in.position * 2.0 - 1.0, 0.0, 1.0);
    out.texcoord = in.texcoord;
    return out;
}
