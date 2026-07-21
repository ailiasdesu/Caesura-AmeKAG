// ==================================================================
// Caesura (AmeKAG) - Embedded MiniGame 3D Shaders (OpenGL GLSL 430)
// ==================================================================
// Phong lighting: 1 directional + up to 3 point lights + PBR params
// ==================================================================

#include "EmbeddedShaders.h"

namespace Caesura {

// -- MiniGame Vertex Shader (GLSL) -------------------------------------
const char* kEmbeddedGLSL_MiniGame_VS = R"(
#version 430 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;

uniform mat4 u_mtx;       // model matrix
uniform mat4 u_viewProj;  // view-projection matrix

out vec3 v_worldPos;
out vec3 v_normal;

void main() {
    vec4 worldPos = u_mtx * vec4(a_position, 1.0);
    v_worldPos = worldPos.xyz;
    v_normal = normalize(mat3(u_mtx) * a_normal);
    gl_Position = u_viewProj * worldPos;
}
)";

// -- MiniGame Fragment Shader (GLSL) ------------------------------------
const char* kEmbeddedGLSL_MiniGame_FS = R"(
#version 430 core

in vec3 v_worldPos;
in vec3 v_normal;

uniform vec4 u_albedo;
uniform vec4 u_lightDir;     // directional light: xyz=direction, w=intensity
uniform vec4 u_lightColor;   // directional light color
uniform vec4 u_ambient;      // ambient color
uniform vec4 u_cameraPos;    // camera world position
uniform vec4 u_material;     // x=roughness, y=metallic, z=specular, w=unused
uniform vec4 u_viewProj[4];  // view-projection matrix (4 rows)

// Point lights (up to 3)
uniform vec4 u_lightPos0;    // xyz=position, w=intensity
uniform vec4 u_lightCol0;    // rgb=color, w=range
uniform vec4 u_lightPos1;
uniform vec4 u_lightCol1;
uniform vec4 u_lightPos2;
uniform vec4 u_lightCol2;
uniform vec4 u_lightCount;   // x=active count

out vec4 fragColor;

vec3 calcDirectional(vec3 N, vec3 V, vec3 albedo, float roughness, float metallic, float specular) {
    vec3 L = normalize(-u_lightDir.xyz);
    vec3 H = normalize(L + V);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    // Fresnel-Schlick approximation
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 fresnel = F0 + (1.0 - F0) * pow(1.0 - max(dot(H, V), 0.0), 5.0);

    float D = (specular + 2.0) / (2.0 * 3.14159) * pow(NdotH, specular);

    vec3 diffuse = albedo * NdotL / 3.14159;
    vec3 spec = fresnel * D * NdotL;

    return (diffuse + spec) * u_lightColor.rgb * u_lightDir.w;
}

vec3 calcPointLight(int i, vec3 N, vec3 V, vec3 albedo, float roughness, float metallic, float specular) {
    vec4 pos, col;
    if (i == 0) { pos = u_lightPos0; col = u_lightCol0; }
    else if (i == 1) { pos = u_lightPos1; col = u_lightCol1; }
    else { pos = u_lightPos2; col = u_lightCol2; }

    vec3 lightVec = pos.xyz - v_worldPos;
    float dist = length(lightVec);
    float attenuation = max(1.0 - dist / col.w, 0.0) * pos.w;

    vec3 L = normalize(lightVec);
    vec3 H = normalize(L + V);
    float NdotL = max(dot(N, L), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 fresnel = F0 + (1.0 - F0) * pow(1.0 - max(dot(H, V), 0.0), 5.0);
    float D = (specular + 2.0) / (2.0 * 3.14159) * pow(max(dot(N, H), 0.0), specular);

    vec3 diffuse = albedo * NdotL / 3.14159;
    vec3 spec = fresnel * D * NdotL;

    return (diffuse + spec) * col.rgb * attenuation;
}

void main() {
    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_cameraPos.xyz - v_worldPos);
    vec3 albedo = u_albedo.rgb;
    float roughness = u_material.x;
    float metallic = u_material.y;
    float specular = u_material.z * 128.0;

    vec3 color = u_ambient.rgb * albedo;
    color += calcDirectional(N, V, albedo, roughness, metallic, specular);

    int lightCount = int(u_lightCount.x);
    for (int i = 0; i < lightCount && i < 3; i++) {
        color += calcPointLight(i, N, V, albedo, roughness, metallic, specular);
    }

    fragColor = vec4(color, u_albedo.a);
}
)";

const char* Caesura_GetMiniGameVS_GLSL() { return kEmbeddedGLSL_MiniGame_VS; }
const char* Caesura_GetMiniGameFS_GLSL() { return kEmbeddedGLSL_MiniGame_FS; }

} // namespace Caesura
