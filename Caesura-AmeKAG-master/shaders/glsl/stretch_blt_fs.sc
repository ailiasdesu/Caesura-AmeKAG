// Caesura (AmeKAG) - Stretch Blit Fragment Shader (GLSL)
// bgfx shaderc variant: filtered copy from src texture
// Corresponds to spec [2.4] stretch_blt.fs

$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
uniform vec4 u_stretchParams;     // xy = src_offset, zw = src_scale

void main()
{
    vec2 uv = u_stretchParams.xy + v_texcoord0 * u_stretchParams.zw;
    gl_FragColor = texture2D(s_texColor, uv);
}
