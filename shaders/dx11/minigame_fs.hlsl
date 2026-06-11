// Caesura (AmeKAG) - MiniGame 3D Fragment Shader (HLSL ps_4_0)
// PBR-lite with 1 directional + up to 3 point lights
// IU-4: Multi-light system

struct PSInput {
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
};

// -- Directional light (always active) --
uniform float4 u_lightDir;     // xyz=direction, w=intensity
uniform float4 u_lightColor;   // rgb=color

// -- Point lights (up to 3, indexed 0..2) --
uniform float4 u_lightPos0;    // xyz=pos, w=range
uniform float4 u_lightCol0;    // rgb=color, w=intensity
uniform float4 u_lightPos1;
uniform float4 u_lightCol1;
uniform float4 u_lightPos2;
uniform float4 u_lightCol2;

uniform float4 u_lightCount;   // x = number of active point lights (0-3)

// -- Material + camera --
uniform float4 u_albedo;
uniform float4 u_ambient;
uniform float4 u_cameraPos;
uniform float4 u_material;     // x=roughness, y=metallic, z=specStr

float3 calcPointLight(float3 pos, float range, float3 color, float intensity,
                      float3 worldPos, float3 N, float3 V, float3 albedo,
                      float roughness, float metallic, float specStr) {
    float3 Lvec = pos - worldPos;
    float dist = length(Lvec);
    if (dist > range) return float3(0,0,0);

    float3 L = Lvec / dist;
    float3 H = normalize(L + V);

    // Attenuation: inverse square with smooth falloff
    float atten = 1.0 - smoothstep(0.0, range, dist);
    atten *= atten;

    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    float shininess = pow(2.0, (1.0 - roughness) * 10.0);
    float3 specColor = lerp(float3(1,1,1), albedo, metallic);

    float3 diffuse  = color * albedo * NdotL;
    float3 specular = color * specColor * pow(NdotH, shininess) * specStr;
    specular *= (1.0 - roughness * 0.5);

    return (diffuse + specular) * intensity * atten;
}

float4 main(PSInput input) : SV_TARGET
{
    float3 N = normalize(input.normal);
    float3 V = normalize(u_cameraPos.xyz - input.worldPos);

    float roughness = u_material.x;
    float metallic  = u_material.y;
    float specStr   = u_material.z;

    // -- Directional light --
    float3 Ldir = normalize(u_lightDir.xyz);
    float3 Hdir = normalize(Ldir + V);
    float NdotL_dir = max(dot(N, Ldir), 0.0);
    float NdotH_dir = max(dot(N, Hdir), 0.0);
    float shininess = pow(2.0, (1.0 - roughness) * 10.0);
    float3 specColor = lerp(float3(1,1,1), u_albedo.rgb, metallic);

    float3 ambient  = u_ambient.rgb * u_albedo.rgb;
    float3 diffuse  = u_lightColor.rgb * u_albedo.rgb * NdotL_dir;
    float3 specular = u_lightColor.rgb * specColor * pow(NdotH_dir, shininess) * specStr;
    specular *= (1.0 - roughness * 0.5);

    float3 color = ambient + (diffuse + specular) * u_lightDir.w;

    // -- Point lights --
    int count = int(u_lightCount.x);
    if (count > 0) {
        color += calcPointLight(u_lightPos0.xyz, u_lightPos0.w, u_lightCol0.rgb, u_lightCol0.w,
                                input.worldPos, N, V, u_albedo.rgb,
                                roughness, metallic, specStr);
    }
    if (count > 1) {
        color += calcPointLight(u_lightPos1.xyz, u_lightPos1.w, u_lightCol1.rgb, u_lightCol1.w,
                                input.worldPos, N, V, u_albedo.rgb,
                                roughness, metallic, specStr);
    }
    if (count > 2) {
        color += calcPointLight(u_lightPos2.xyz, u_lightPos2.w, u_lightCol2.rgb, u_lightCol2.w,
                                input.worldPos, N, V, u_albedo.rgb,
                                roughness, metallic, specStr);
    }

    return float4(color, 1.0);
}
