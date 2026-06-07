// Caesura (AmeKAG) - 2D Texture Fragment Shader (Metal)
// Corresponds to shaders/dx11/fs_texture.hlsl

#include <metal_stdlib>
using namespace metal;

struct PSInput {
    float4 position [[position]];
    float2 texcoord;
};

fragment float4 fs_texture(PSInput in [[stage_in]],
                           texture2d<float> s_texture [[texture(0)]],
                           sampler s_textureSampler [[sampler(0)]]) {
    return s_texture.sample(s_textureSampler, in.texcoord);
}
