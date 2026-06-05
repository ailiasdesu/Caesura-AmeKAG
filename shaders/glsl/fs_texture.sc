// Caesura (AmeKAG) - 2D Texture Fragment Shader (GLSL)
// bgfx shaderc variant
// Corresponds to shaders/dx11/fs_texture.hlsl

$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texture, 0);

void main()
{
    gl_FragColor = texture2D(s_texture, v_texcoord0);
}
