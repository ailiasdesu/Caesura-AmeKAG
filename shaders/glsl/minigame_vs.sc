// Caesura (AmeKAG) - MiniGame 3D Vertex Shader (Phong)
// bgfx shaderc .sc variant
// Input:  position + normal
// Output: world-space position + normal for fragment lighting

$input a_position, a_normal
$output v_worldPos, v_normal

#include <bgfx_shader.sh>

void main()
{
    vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0));
    v_worldPos = worldPos.xyz;
    // Transform normal to world space (assumes uniform scale)
    v_normal = normalize(mul(u_model[0], vec4(a_normal, 0.0)).xyz);
    gl_Position = mul(u_viewProj, worldPos);
}
