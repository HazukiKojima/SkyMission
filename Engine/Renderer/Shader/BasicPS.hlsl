#include "CommonLighting.hlsli"

cbuffer MatrixBuffer : register(b0)
{
    float4x4 mvp;
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

Texture2D gDiffuse : register(t0);
SamplerState gSampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float3 wavePos : TEXCOORD2;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

float4 PS(PS_INPUT input) : SV_TARGET
{
    CommonLightingParameters lighting;

    lighting.sunDirection = sunDirection;
    lighting.sunIntensity = sunIntensity;
    lighting.sunColor = sunColor;
    lighting.ambientIntensity = ambientIntensity;
    lighting.ambientColor = ambientColor;
    lighting.padding = 0.0f;

    // 表面の基底から拡散光と鏡面反射を求める
    float3 V = SafeNormalize(cameraPos - input.worldPos);
    float3 L = SafeNormalize(lighting.sunDirection);

    float3 N;
    float3 T;
    float3 B;

    BuildTBN(input.normal, input.tangent, N, T, B);

    float NdotL = saturate(dot(N, L));

    float3 albedo = gDiffuse.Sample(gSampler, input.texcoord).rgb;

    float3 diffuse = albedo * NdotL;

    float3 ambient = albedo * GetAmbientRadiance(lighting);

    float3 sunSpecular = CalculateSunSpecular(N, V, L, 0.35f, lighting);

    float3 color = diffuse + ambient + sunSpecular;

    // 距離に応じた大気の霧

    float distanceToCamera = length(cameraPos - input.worldPos);
    float fogStart = 1500.0f;
    float fogEnd = 12000.0f;

    float fogFactor = saturate((distanceToCamera - fogStart) / (fogEnd - fogStart));

    float3 atmosphere = GetSkyColor(V, lighting);

    color = lerp(color, atmosphere, fogFactor);

    // HDR色を表示範囲へ圧縮する

    color = ToneMapReinhard(color);

    return float4(saturate(color), 1.0f);
}