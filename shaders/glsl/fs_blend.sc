// Caesura (AmeKAG) - Multi-mode Blend Fragment Shader (GLSL)
// bgfx shaderc variant
// Corresponds to shaders/dx11/fs_blend.hlsl

$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_baseTex,  0);
SAMPLER2D(s_blendTex, 1);

uniform vec4 u_blendParams;       // x=baseAlpha, y=blendAlpha, z=globalAlpha, w=mode
#define u_opacity    u_blendParams.x
#define u_blendAlpha u_blendParams.y
#define u_globalAlpha u_blendParams.z
#define u_blendMode  int(u_blendParams.w)

vec3 bAdd(vec3 a, vec3 b) { return a + b; }
vec3 bSub(vec3 a, vec3 b) { return a - b; }
vec3 bMul(vec3 a, vec3 b) { return a * b; }
vec3 bScr(vec3 a, vec3 b) { return vec3(1.0) - (vec3(1.0) - a) * (vec3(1.0) - b); }
vec3 bOvl(vec3 a, vec3 b) {
    vec3 r;
    r.r = a.r < 0.5 ? 2.0 * a.r * b.r : 1.0 - 2.0 * (1.0 - a.r) * (1.0 - b.r);
    r.g = a.g < 0.5 ? 2.0 * a.g * b.g : 1.0 - 2.0 * (1.0 - a.g) * (1.0 - b.g);
    r.b = a.b < 0.5 ? 2.0 * a.b * b.b : 1.0 - 2.0 * (1.0 - a.b) * (1.0 - b.b);
    return r;
}
vec3 bDrk(vec3 a, vec3 b) { return min(a, b); }
vec3 bLit(vec3 a, vec3 b) { return max(a, b); }
vec3 bDif(vec3 a, vec3 b) { return abs(a - b); }

void main()
{
    vec4 base  = texture2D(s_baseTex,  v_texcoord0) * u_opacity;
    vec4 blend = texture2D(s_blendTex, v_texcoord0) * u_blendAlpha;
    vec3 c;

    if (u_blendMode == 1)       c = bAdd(base.rgb, blend.rgb);
    else if (u_blendMode == 2)  c = bSub(base.rgb, blend.rgb);
    else if (u_blendMode == 3)  c = bMul(base.rgb, blend.rgb);
    else if (u_blendMode == 4)  c = bScr(base.rgb, blend.rgb);
    else if (u_blendMode == 5)  c = bOvl(base.rgb, blend.rgb);
    else if (u_blendMode == 6)  c = bDrk(base.rgb, blend.rgb);
    else if (u_blendMode == 7)  c = bLit(base.rgb, blend.rgb);
    else if (u_blendMode == 8)  c = bDif(base.rgb, blend.rgb);
    else                        c = mix(base.rgb, blend.rgb, blend.a);

    float alpha = clamp(base.a + blend.a, 0.0, 1.0);
    gl_FragColor = vec4(c, alpha) * u_globalAlpha;
}
