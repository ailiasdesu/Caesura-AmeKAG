// Caesura (AmeKAG) - MiniGame 3D Fragment Shader (Phong)
// bgfx shaderc .sc variant
// Per-pixel Phong: ambient + diffuse N·L + Blinn-Phong specular
// Single directional light (IU-4 expands to multi-light array)

$input v_worldPos, v_normal

#include <bgfx_shader.sh>

uniform vec4 u_albedo;
uniform vec4 u_lightDir;      // xyz = direction to light, w = intensity
uniform vec4 u_lightColor;    // rgb = color
uniform vec4 u_ambient;       // rgb = ambient color
uniform vec4 u_cameraPos;     // xyz = camera world position

void main()
{
    vec3 N = normalize(v_normal);
    vec3 L = normalize(u_lightDir.xyz);
    vec3 V = normalize(u_cameraPos.xyz - v_worldPos);
    vec3 H = normalize(L + V);   // Blinn-Phong half-vector

    vec3 ambient  = u_ambient.rgb * u_albedo.rgb;
    vec3 diffuse  = u_lightColor.rgb * u_albedo.rgb * max(dot(N, L), 0.0);
    vec3 specular = u_lightColor.rgb * pow(max(dot(N, H), 0.0), 64.0) * 0.5;

    vec3 color = ambient + (diffuse + specular) * u_lightDir.w;

    gl_FragColor = vec4(color, 1.0);
}
