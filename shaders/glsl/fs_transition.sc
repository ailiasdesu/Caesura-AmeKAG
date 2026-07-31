// Caesura (AmeKAG) - Transition Fragment Shader (GLSL)
// bgfx shaderc variant: crossfade / rule / wipe between two textures
// Corresponds to shaders/dx11/fs_transition.hlsl

$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texture,  0);
SAMPLER2D(s_texture1, 1);
SAMPLER2D(s_texture2, 2);

uniform vec4 TransParams;         // x=progress, y=method, z=pad, w=pad
#define u_progress TransParams.x
#define u_method  int(TransParams.y)

void main()
{
    vec4 fc = texture2D(s_texture, v_texcoord0);
    vec4 tc = texture2D(s_texture1,   v_texcoord0);
    float t = clamp(u_progress, 0.0, 1.0);

    if (u_method == 1)      t = step(texture2D(s_texture2, v_texcoord0).r, t);
    else if (u_method == 2) t = step(v_texcoord0.x, t);
    else if (u_method == 3) t = step(1.0 - v_texcoord0.x, t);
    else if (u_method == 4) t = step(1.0 - v_texcoord0.y, t);
    else if (u_method == 5) t = step(v_texcoord0.y, t);

    gl_FragColor = mix(fc, tc, t);
}
