// Caesura (AmeKAG) - Stretch Blit Fragment Shader (GLSL)
// bgfx shaderc variant: filtered copy from src texture
// Corresponds to spec [2.4] stretch_blt.fs

$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texture, 0);
uniform vec4 StretchParams;       // xy = src_offset, zw = src_scale


#if BGFX_SHADER_LANGUAGE_GLSL
out vec4 bgfx_FragColor;
#endif
void main()
{
    vec2 uv = StretchParams.xy + v_texcoord0 * StretchParams.zw;
    #if BGFX_SHADER_LANGUAGE_GLSL

    bgfx_FragColor = texture2D(s_texture, uv);

    #else

    gl_FragColor = texture2D(s_texture, uv);

    #endif
}
