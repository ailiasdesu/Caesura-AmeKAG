// Caesura (AmeKAG) - LUT color-grade FS (GLSL, bgfx shaderc)
$input v_texcoord0
#include <bgfx_shader.sh>
SAMPLER2D(s_texture, 0);
uniform vec4 PostFxParams[4]; // [0]=strength,0,0,lutMix [1]=rgb grade [2]=texel [3]=spare

#if BGFX_SHADER_LANGUAGE_GLSL
out vec4 bgfx_FragColor;
#endif
void main() {
    vec2 uv = v_texcoord0;
    vec4 src = texture2D(s_texture, uv);
    vec3 mult = max(PostFxParams[1].rgb, vec3(0.0));
    vec3 graded = src.rgb * mult;
    float luma = dot(src.rgb, vec3(0.2126, 0.7152, 0.0722));
    vec3 boost = mix(vec3(luma), src.rgb * mult, 0.5);
    vec3 mixed = mix(src.rgb, graded, PostFxParams[0].w);
    mixed = mix(mixed, boost, PostFxParams[0].x * 0.5);
    #if BGFX_SHADER_LANGUAGE_GLSL

    bgfx_FragColor = vec4(mixed, src.a);

    #else

    gl_FragColor = vec4(mixed, src.a);

    #endif
}
