// Caesura (AmeKAG) - Affine Blit Fragment Shader (GLSL)
// bgfx shaderc variant: passthrough texture lookup
// Corresponds to spec [2.4] (uses same FS as stretch_blt)

$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texture, 0);


#if BGFX_SHADER_LANGUAGE_GLSL
out vec4 bgfx_FragColor;
#endif
void main()
{
    #if BGFX_SHADER_LANGUAGE_GLSL

    bgfx_FragColor = texture2D(s_texture, v_texcoord0);

    #else

    gl_FragColor = texture2D(s_texture, v_texcoord0);

    #endif
}
