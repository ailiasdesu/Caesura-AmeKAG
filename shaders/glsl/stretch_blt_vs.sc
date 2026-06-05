// Caesura (AmeKAG) - Stretch Blit Vertex Shader (GLSL)
// bgfx shaderc variant: src/dst rect quad mapping
// Corresponds to spec [2.4] stretch_blt

$input a_position, a_texcoord0
$output v_texcoord0

#include <bgfx_shader.sh>

void main()
{
    gl_Position = vec4(a_position * 2.0 - 1.0, 0.0, 1.0);
    v_texcoord0 = a_texcoord0;
}
