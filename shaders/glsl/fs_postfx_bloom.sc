// Caesura (AmeKAG) - Bloom composite FS (GLSL, bgfx shaderc)
$input v_texcoord0
#include <bgfx_shader.sh>
SAMPLER2D(s_texture,  0);
SAMPLER2D(s_texture1, 1);
uniform vec4 PostFxParams[4]; // [0]=strength,0,threshold,0 [1]=tint [2]=texel [3]=spare
void main() {
    vec2 uv = v_texcoord0;
    vec4 src = texture2D(s_texture, uv);
    float thr = max(PostFxParams[0].z, 0.0);
    float lum = dot(src.rgb, vec3(0.2126, 0.7152, 0.0722));
    vec3 bright = max(lum - thr, 0.0) * src.rgb;
    vec3 bloom = texture2D(s_texture1, uv).rgb;
    vec3 add = (bloom + bright) * max(PostFxParams[0].x, 0.0);
    add *= max(PostFxParams[1].rgb, vec3(0.0));
    gl_FragColor = vec4(clamp(src.rgb + add, 0.0, 1.0), src.a);
}
