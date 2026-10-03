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

Texture2D gSkyTexture : register(t0);
SamplerState gSampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    CommonLightingParameters lighting;
    lighting.sunDirection = sunDirection;
    lighting.sunIntensity = sunIntensity;
    lighting.sunColor = sunColor;
    lighting.ambientIntensity = ambientIntensity;
    lighting.ambientColor = ambientColor;
    lighting.padding = 0.0f;

    // テクスチャのサンプリング
    float3 rawColor = gSkyTexture.Sample(gSampler, input.texcoord).rgb;
    
    // 露出とベースの明るい空色のブレンド
    float exposure = 1.0f;
    float3 color = rawColor * exposure;
    
    // 太陽と反対側が暗く沈むのを防ぐため、明るい青を加算
    color = max(color, float3(0.22f, 0.40f, 0.65f));

    float3 skyDirection = SafeNormalize(input.worldPos - cameraPos);
    color += CalculateSunContribution(skyDirection, lighting, 256.0f) * 2.0f;

    // トーンマッピング
    color = ToneMapReinhard(color);
    
    return float4(max(color, 0.0f), 1.0f);
}
