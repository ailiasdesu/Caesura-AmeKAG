// Caesura (AmeKAG) - Fullscreen Quad Vertex Shader (GLSL)
// bgfx shaderc variant: procedural quad from vertex ID
// Corresponds to shaders/dx11/vs_fullscreen.hlsl

$output v_texcoord0

#include <bgfx_shader.sh>

void main()
{
    // bgfx uses gl_VertexID-style procedural quad
    vec2 uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(uv * vec2(2.0, -2.0) + vec2(-1.0, 1.0), 0.0, 1.0);
    v_texcoord0 = uv * vec2(0.5, -0.5) + vec2(0.0, 1.0);
}
