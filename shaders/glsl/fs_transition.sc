// Caesura (AmeKAG) - Transition Fragment Shader (GLSL)
// bgfx shaderc variant: crossfade / rule / wipe between two textures
// Corresponds to shaders/dx11/fs_transition.hlsl

$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_fromTex, 0);
SAMPLER2D(s_toTex,   1);
SAMPLER2D(s_ruleTex, 2);

uniform vec4 u_transParams;       // x=progress, y=method, z=pad, w=pad
#define u_progress u_transParams.x
#define u_method  int(u_transParams.y)

void main()
{
    vec4 fc = texture2D(s_fromTex, v_texcoord0);
    vec4 tc = texture2D(s_toTex,   v_texcoord0);
    float t = clamp(u_progress, 0.0, 1.0);

    if (u_method == 1)      t = step(texture2D(s_ruleTex, v_texcoord0).r, t);
    else if (u_method == 2) t = step(v_texcoord0.x, t);
    else if (u_method == 3) t = step(1.0 - v_texcoord0.x, t);
    else if (u_method == 4) t = step(1.0 - v_texcoord0.y, t);
    else if (u_method == 5) t = step(v_texcoord0.y, t);

    gl_FragColor = mix(fc, tc, t);
}
