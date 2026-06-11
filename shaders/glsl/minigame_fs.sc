// Caesura (AmeKAG) - MiniGame 3D Fragment Shader (PBR-lite)
// bgfx shaderc .sc variant — matches DX11 HLSL feature set
// Cook-Torrance BRDF: 1 directional + up to 3 point lights
// roughness / metallic / specStr material model

$input v_worldPos, v_normal

#include <bgfx_shader.sh>

// -- Directional light --
uniform vec4 u_lightDir;      // xyz = direction to light, w = intensity
uniform vec4 u_lightColor;    // rgb = color

// -- Point lights (up to 3, indexed 0..2) --
uniform vec4 u_lightPos0;     // xyz = position, w = range
uniform vec4 u_lightCol0;     // rgb = color, w = intensity
uniform vec4 u_lightPos1;
uniform vec4 u_lightCol1;
uniform vec4 u_lightPos2;
uniform vec4 u_lightCol2;

uniform vec4 u_lightCount;    // x = number of active point lights (0-3)

// -- Material + camera --
uniform vec4 u_albedo;
uniform vec4 u_ambient;
uniform vec4 u_cameraPos;
uniform vec4 u_material;      // x = roughness, y = metallic, z = specStr

vec3 calcPointLight(vec3 pos, float range, vec3 color, float intensity,
                    vec3 worldPos, vec3 N, vec3 V, vec3 albedo,
                    float roughness, float metallic, float specStr) {
    vec3 Lvec = pos - worldPos;
    float dist = length(Lvec);
    if (dist > range) return vec3(0.0);

    vec3 L = Lvec / dist;
    vec3 H = normalize(L + V);

    // Attenuation: inverse square with smooth falloff
    float atten = 1.0 - smoothstep(0.0, range, dist);
    atten *= atten;

    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    float shininess = pow(2.0, (1.0 - roughness) * 10.0);
    vec3 specColor = mix(vec3(1.0), albedo, metallic);

    vec3 diffuse  = color * albedo * NdotL;
    vec3 specular = color * specColor * pow(NdotH, shininess) * specStr;
    specular *= (1.0 - roughness * 0.5);

    return (diffuse + specular) * intensity * atten;
}

void main()
{
    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_cameraPos.xyz - v_worldPos);

    float roughness = u_material.x;
    float metallic  = u_material.y;
    float specStr   = u_material.z;

    // -- Directional light --
    vec3 Ldir = normalize(u_lightDir.xyz);
    vec3 Hdir = normalize(Ldir + V);
    float NdotL_dir = max(dot(N, Ldir), 0.0);
    float NdotH_dir = max(dot(N, Hdir), 0.0);
    float shininess = pow(2.0, (1.0 - roughness) * 10.0);
    vec3 specColor = mix(vec3(1.0), u_albedo.rgb, metallic);

    vec3 ambient  = u_ambient.rgb * u_albedo.rgb;
    vec3 diffuse  = u_lightColor.rgb * u_albedo.rgb * NdotL_dir;
    vec3 specular = u_lightColor.rgb * specColor * pow(NdotH_dir, shininess) * specStr;
    specular *= (1.0 - roughness * 0.5);

    vec3 color = ambient + (diffuse + specular) * u_lightDir.w;

    // -- Point lights --
    int count = int(u_lightCount.x);
    if (count > 0) {
        color += calcPointLight(u_lightPos0.xyz, u_lightPos0.w, u_lightCol0.rgb, u_lightCol0.w,
                                v_worldPos, N, V, u_albedo.rgb,
                                roughness, metallic, specStr);
    }
    if (count > 1) {
        color += calcPointLight(u_lightPos1.xyz, u_lightPos1.w, u_lightCol1.rgb, u_lightCol1.w,
                                v_worldPos, N, V, u_albedo.rgb,
                                roughness, metallic, specStr);
    }
    if (count > 2) {
        color += calcPointLight(u_lightPos2.xyz, u_lightPos2.w, u_lightCol2.rgb, u_lightCol2.w,
                                v_worldPos, N, V, u_albedo.rgb,
                                roughness, metallic, specStr);
    }

    gl_FragColor = vec4(color, 1.0);
}