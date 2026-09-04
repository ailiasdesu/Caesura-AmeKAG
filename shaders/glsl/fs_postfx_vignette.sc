// Caesura (AmeKAG) - Vignette FS (GLSL, bgfx shaderc)
// Radial darkening toward corners with optional rgb tint.
$input v_texcoord0
#include <bgfx_shader.sh>
SAMPLER2D(s_texture, 0);
uniform vec4 PostFxParams[4]; // [0]=strength,innerR,0,0 [1]=tint rgb [2]=texel [3]=spare

#if BGFX_SHADER_LANGUAGE_GLSL
out vec4 bgfx_FragColor;
#endif
void main() {
    vec2 uv = v_texcoord0;
    vec4 src = texture2D(s_texture, uv);
    vec2 cc = uv - vec2(0.5, 0.5);
    float r = length(cc) * 2.0;
    float inner = max(PostFxParams[0].y, 0.0001);
    float edge = 1.7;
    float v = clamp((r - inner) / (edge - inner), 0.0, 1.0);
    vec3 tint = max(PostFxParams[1].rgb, vec3(0.0));
    vec3 vign = mix(src.rgb, src.rgb * tint, v);
    vec3 outC = mix(src.rgb, vign, PostFxParams[0].x);
    #if BGFX_SHADER_LANGUAGE_GLSL

    bgfx_FragColor = vec4(outC, src.a);

    #else

    gl_FragColor = vec4(outC, src.a);

    #endif
}
