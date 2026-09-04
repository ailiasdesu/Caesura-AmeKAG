// Caesura (AmeKAG) - 3D LUT color-grade FS (GLSL, bgfx shaderc, t214)
$input v_texcoord0
#include <bgfx_shader.sh>
SAMPLER2D(s_texture, 0);
SAMPLER2D(s_lut, 1);
uniform vec4 PostFxParams[4]; // [0]=intensity [2]=(1/w,1/h,N,0)

#if BGFX_SHADER_LANGUAGE_GLSL
out vec4 bgfx_FragColor;
#endif
void main() {
    vec2 uv = v_texcoord0;
    vec4 src = texture2D(s_texture, uv);
    float N = max(floor(PostFxParams[2].z + 0.5), 2.0);
    float col = uv.x * N * N;
    float slice = floor(col / N);
    float sx = col - slice * N + 0.5;
    float sy = uv.y * N + 0.5;
    vec2 luv = vec2(sx / (N * N), sy / N);
    vec4 lut = texture2D(s_lut, luv);
    vec3 outC = mix(src.rgb, lut.rgb, clamp(PostFxParams[0].x, 0.0, 1.0));
    #if BGFX_SHADER_LANGUAGE_GLSL

    bgfx_FragColor = vec4(outC, src.a);

    #else

    gl_FragColor = vec4(outC, src.a);

    #endif
}
