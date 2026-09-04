// Caesura (AmeKAG) - 2D Texture Fragment Shader (GLSL)
// bgfx shaderc variant
// Corresponds to shaders/dx11/fs_texture.hlsl

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
