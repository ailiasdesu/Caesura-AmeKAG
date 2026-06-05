// Caesura (AmeKAG) - Affine Blit Fragment Shader (GLSL)
// bgfx shaderc variant: passthrough texture lookup
// Corresponds to spec [2.4] (uses same FS as stretch_blt)

$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

void main()
{
    gl_FragColor = texture2D(s_texColor, v_texcoord0);
}
