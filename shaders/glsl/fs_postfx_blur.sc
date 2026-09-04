// Caesura (AmeKAG) - Soft Gaussian Blur FS (GLSL, bgfx shaderc)
$input v_texcoord0
#include <bgfx_shader.sh>
SAMPLER2D(s_texture, 0);
uniform vec4 PostFxParams[4]; // [0]=0,radius,amount,0 [1]=tint [2]=texel [3]=spare

#if BGFX_SHADER_LANGUAGE_GLSL
out vec4 bgfx_FragColor;
#endif
void main() {
    vec2 uv = v_texcoord0;
    vec2 texel = PostFxParams[2].xy;
    float radius = max(PostFxParams[0].y, 0.0);
    vec2 off = texel * radius;
    // 3x3 gaussian weights (approximation of separable 2-pass).
    float w[3];
    w[0] = 0.227027; w[1] = 0.1945946; w[2] = 0.1216216;
    vec4 sum = texture2D(s_texture, uv) * w[1];
    for (int x = 1; x <= 2; ++x) {
        float f = w[x];
        sum += texture2D(s_texture, uv + vec2(off.x * float(x), 0.0)) * f;
        sum += texture2D(s_texture, uv - vec2(off.x * float(x), 0.0)) * f;
        sum += texture2D(s_texture, uv + vec2(0.0, off.y * float(x))) * f;
        sum += texture2D(s_texture, uv - vec2(0.0, off.y * float(x))) * f;
    }
    vec4 original = texture2D(s_texture, uv);
    float amount = clamp(PostFxParams[0].z, 0.0, 1.0);
    vec4 mixed = mix(original, sum, amount);
    mixed.rgb *= max(PostFxParams[1].rgb, vec3(0.0));
    #if BGFX_SHADER_LANGUAGE_GLSL

    bgfx_FragColor = vec4(mixed.rgb, original.a);

    #else

    gl_FragColor = vec4(mixed.rgb, original.a);

    #endif
}
