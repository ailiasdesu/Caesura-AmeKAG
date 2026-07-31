// Caesura (AmeKAG) - VFX Fragment Shader (GLSL)
// bgfx shaderc variant: fade / blur / quake post-processing
// Corresponds to shaders/dx11/fs_vfx.hlsl

$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texture, 0);

uniform vec4 VFXParams[3];        // [0]=rgb+fadeAlpha, [1]=blur+quake, [2]=effect
#define u_effect  int(VFXParams[2].x)
#define u_blurR   VFXParams[1].xy
#define u_qx      VFXParams[1].z
#define u_qy      VFXParams[1].w

void main()
{
    vec2 uv = v_texcoord0;
    vec4 c = vec4(0.0);

    if (u_effect == 1) {
        // Fade
        c = mix(texture2D(s_texture, uv), VFXParams[0], VFXParams[0].a);
    } else if (u_effect == 2) {
        // 3x3 box blur
        vec2 stepSize = u_blurR * 0.3333;
        for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
                c += texture2D(s_texture, uv + vec2(float(x), float(y)) * stepSize);
            }
        }
        c /= 9.0;
    } else if (u_effect == 3) {
        // Quake
        c = texture2D(s_texture, uv + vec2(u_qx, u_qy));
    } else {
        // Pass-through
        c = texture2D(s_texture, uv);
    }

    gl_FragColor = c;
}