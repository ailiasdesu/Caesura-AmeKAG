// ==================================================================
// Caesura (AmeKAG) - Embedded MiniGame 3D Shaders (Metal)
// ==================================================================
// Phong lighting: 1 directional + up to 3 point lights + PBR params
// ==================================================================

#include "EmbeddedShaders.h"

namespace Caesura {

// -- MiniGame Vertex Shader (Metal Shading Language) ---------------------
const char* kEmbeddedMSL_MiniGame_VS = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float3 position [[attribute(0)]];
    float3 normal   [[attribute(1)]];
};

struct VertexOut {
    float4 clipPos  [[position]];
    float3 worldPos;
    float3 normal;
};

struct Uniforms {
    float4x4 u_mtx;
    float4x4 u_viewProj;
};

vertex VertexOut miniGameVS(VertexIn in [[stage_in]],
                             constant Uniforms& uniforms [[buffer(1)]]) {
    VertexOut out;
    float4 worldPos = uniforms.u_mtx * float4(in.position, 1.0);
    out.worldPos = worldPos.xyz;
    out.normal = normalize((uniforms.u_mtx * float4(in.normal, 0.0)).xyz);
    out.clipPos = uniforms.u_viewProj * worldPos;
    return out;
}
)";

// -- MiniGame Fragment Shader (Metal) ------------------------------------
const char* kEmbeddedMSL_MiniGame_FS = R"(
#include <metal_stdlib>
using namespace metal;

struct FragIn {
    float4 clipPos  [[position]];
    float3 worldPos;
    float3 normal;
};

struct LightParams {
    float4 u_lightDir;      // xyz=direction, w=intensity
    float4 u_lightColor;
    float4 u_ambient;
    float4 u_cameraPos;
    float4 u_material;      // x=roughness, y=metallic, z=specular
    // Point lights
    float4 u_lightPos0;
    float4 u_lightCol0;
    float4 u_lightPos1;
    float4 u_lightCol1;
    float4 u_lightPos2;
    float4 u_lightCol2;
    float4 u_lightCount;
};

float3 calcDirectional(float3 N, float3 V, float3 albedo, float roughness, float metallic, float specular,
                       float3 lightDir, float3 lightColor, float intensity) {
    float3 L = normalize(-lightDir);
    float3 H = normalize(L + V);
    float NdotL = max(dot(N, L), 0.0f);
    float NdotH = max(dot(N, H), 0.0f);
    float3 F0 = mix(float3(0.04f), albedo, metallic);
    float3 fresnel = F0 + (1.0f - F0) * pow(1.0f - max(dot(H, V), 0.0f), 5.0f);
    float D = (specular + 2.0f) / (2.0f * 3.14159f) * pow(NdotH, specular);
    float3 diffuse = albedo * NdotL / 3.14159f;
    float3 spec = fresnel * D * NdotL;
    return (diffuse + spec) * lightColor * intensity;
}

float3 calcPointLight(float3 N, float3 V, float3 albedo, float roughness, float metallic, float specular,
                      float3 lightPos, float3 lightCol, float intensity, float range, float3 fragPos) {
    float3 lightVec = lightPos - fragPos;
    float dist = length(lightVec);
    float attenuation = max(1.0f - dist / range, 0.0f) * intensity;
    float3 L = normalize(lightVec);
    float3 H = normalize(L + V);
    float NdotL = max(dot(N, L), 0.0f);
    float3 F0 = mix(float3(0.04f), albedo, metallic);
    float3 fresnel = F0 + (1.0f - F0) * pow(1.0f - max(dot(H, V), 0.0f), 5.0f);
    float D = (specular + 2.0f) / (2.0f * 3.14159f) * pow(max(dot(N, H), 0.0f), specular);
    float3 diffuse = albedo * NdotL / 3.14159f;
    float3 spec = fresnel * D * NdotL;
    return (diffuse + spec) * lightCol * attenuation;
}

fragment float4 miniGameFS(FragIn in [[stage_in]],
                           constant LightParams& lp [[buffer(2)]],
                           constant float4& u_albedo [[buffer(3)]]) {
    float3 N = normalize(in.normal);
    float3 V = normalize(lp.u_cameraPos.xyz - in.worldPos);
    float3 albedo = u_albedo.rgb;
    float roughness = lp.u_material.x;
    float metallic = lp.u_material.y;
    float specular = lp.u_material.z * 128.0f;

    float3 color = lp.u_ambient.rgb * albedo;
    color += calcDirectional(N, V, albedo, roughness, metallic, specular,
                              lp.u_lightDir.xyz, lp.u_lightColor.rgb, lp.u_lightDir.w);

    int lightCount = int(lp.u_lightCount.x);
    if (lightCount > 0) color += calcPointLight(N, V, albedo, roughness, metallic, specular, lp.u_lightPos0.xyz, lp.u_lightCol0.rgb, lp.u_lightPos0.w, lp.u_lightCol0.w, in.worldPos);
    if (lightCount > 1) color += calcPointLight(N, V, albedo, roughness, metallic, specular, lp.u_lightPos1.xyz, lp.u_lightCol1.rgb, lp.u_lightPos1.w, lp.u_lightCol1.w, in.worldPos);
    if (lightCount > 2) color += calcPointLight(N, V, albedo, roughness, metallic, specular, lp.u_lightPos2.xyz, lp.u_lightCol2.rgb, lp.u_lightPos2.w, lp.u_lightCol2.w, in.worldPos);

    return float4(color, u_albedo.a);
}
)";

const char* Caesura_GetMiniGameVS_MSL() { return kEmbeddedMSL_MiniGame_VS; }
const char* Caesura_GetMiniGameFS_MSL() { return kEmbeddedMSL_MiniGame_FS; }

} // namespace Caesura
