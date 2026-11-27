#include "CommonLighting.hlsli"

cbuffer MatrixBuffer : register(b0)
{
    float4x4 mvp;
    float4x4 world;
    float time;
    float3 padding;
    float3 cameraPos;
    float pad2;
    float3 sunDirection;
    float sunIntensity;
    float3 sunColor;
    float ambientIntensity;
    float3 ambientColor;
    float pad3;
};

Texture2D gBaseColor : register(t0);
Texture2D gMetallic : register(t1);
Texture2D gRoughness : register(t2);
Texture2D gNormal : register(t3);
SamplerState gSampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
};

float4 PS(PS_INPUT input) : SV_TARGET
{
    const float minimumMaterialRoughness = 0.04f;
    const float minimumViewNormalDot = 1e-4f;
    const float3 dielectricReflectance = float3(0.04f, 0.04f, 0.04f);
    const float modelAmbientBoost = 1.8f;
    const float modelExposure = 1.5f;
    const float modelShadowLiftExponent = 0.76f;
    CommonLightingParameters lighting;
    lighting.sunDirection = sunDirection;
    lighting.sunIntensity = sunIntensity;
    lighting.sunColor = sunColor;
    lighting.ambientIntensity = ambientIntensity;
    lighting.ambientColor = ambientColor;
    lighting.padding = 0.0f;

    float4 albedo = gBaseColor.Sample(gSampler, input.texcoord);
    float metallic = saturate(gMetallic.Sample(gSampler, input.texcoord).r);
    float roughness = clamp(gRoughness.Sample(gSampler, input.texcoord).r, minimumMaterialRoughness, 1.0f);

    // 法線画像のZを復元し、接空間からワールド空間へ変換する
    float3 N = SafeNormalize(input.normal);
    float3 T = SafeNormalize(input.tangent.xyz - N * dot(input.tangent.xyz, N));
    float3 B = SafeNormalize(cross(N, T)) * input.tangent.w;
    float2 tangentNormalXY = gNormal.Sample(gSampler, input.texcoord).rg * 2.0f - 1.0f;
    float3 tangentNormal =
        float3(tangentNormalXY, sqrt(saturate(1.0f - dot(tangentNormalXY, tangentNormalXY))));
    N = SafeNormalize(T * tangentNormal.x + B * tangentNormal.y + N * tangentNormal.z);

    float3 L = SafeNormalize(lighting.sunDirection);
    float3 V = SafeNormalize(cameraPos - input.worldPos);
    float3 H = SafeNormalize(V + L);
    float NdotL = saturate(dot(N, L));
    float NdotV = max(saturate(dot(N, V)), minimumViewNormalDot);
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    // 金属度と粗さから鏡面反射と拡散反射を求める
    float3 F0 = lerp(dielectricReflectance, albedo.rgb, metallic);
    float3 F = FresnelSchlick(VdotH, F0);
    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    float3 specular = D * G * F / max(4.0f * NdotV * NdotL, kMinimumSpecularDenominator);
    float3 diffuseWeight = (1.0f - F) * (1.0f - metallic);
    float3 diffuse = diffuseWeight * albedo.rgb / PI;
    float3 directLighting = (diffuse + specular) * GetSunRadiance(lighting) * NdotL;
    float3 ambientLighting = lerp(albedo.rgb, F0, metallic) * GetAmbientRadiance(lighting) * modelAmbientBoost;
    float3 color = (directLighting + ambientLighting) * modelExposure;

    // 空や海の露出を変えずに、機体マテリアルの暗部を持ち上げる
    color = pow(saturate(color), modelShadowLiftExponent);

    return float4(color, albedo.a);
}
