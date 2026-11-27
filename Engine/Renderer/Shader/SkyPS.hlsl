#include "CommonLighting.hlsli"

// CPUから受け取る描画定数

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

// 参照するテクスチャとサンプラー

Texture2D gSkyTexture : register(t0);
SamplerState gSampler : register(s0);

// 頂点シェーダーから受け取る値

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
};

// 画素の色を計算する

float4 PS(PS_INPUT input) : SV_TARGET
{
    const float horizonAtmosphereExponent = 3.0f;
    const float horizonTextureWeight = 0.72f;
    const float horizonAtmosphereWeight = 0.28f;
    const float horizonGlowExponent = 7.0f;
    const float3 horizonGlowColor = float3(0.025f, 0.035f, 0.045f);
    const float sunDiskExponent = 18000.0f;
    const float sunGlowExponent = 180.0f;
    const float sunDiskStrength = 0.65f;
    const float sunGlowStrength = 0.035f;
    // 照明パラメーターを組み立てる

    CommonLightingParameters lighting;

    lighting.sunDirection = sunDirection;
    lighting.sunIntensity = sunIntensity;
    lighting.sunColor = sunColor;
    lighting.ambientIntensity = ambientIntensity;
    lighting.ambientColor = ambientColor;
    lighting.padding = 0.0f;

    // HDRの空テクスチャを参照する

    float3 hdrColor = gSkyTexture.Sample(gSampler, input.texcoord).rgb;

    // 露出を調整する

    const float exposure = 1.1f;

    float3 color = hdrColor * exposure;

    // 地平線付近の大気散乱

    float3 skyDirection = SafeNormalize(input.worldPos - cameraPos);

    float height = saturate(skyDirection.y);

    // 地平線付近だけ少し大気色を混ぜる
    float horizonFactor = pow(1.0f - height, horizonAtmosphereExponent);

    float3 atmosphericColor = GetSkyColor(skyDirection, lighting);

    // HDRテクスチャと大気色を合成し、空の明暗差を抑える
    const float hdrBlend = 0.65f;

    color = lerp(atmosphericColor, color, hdrBlend);

    color = lerp(
        color, color * horizonTextureWeight + atmosphericColor * horizonAtmosphereWeight, horizonFactor);

    // 地平線付近を明るくする

    float horizonGlow = pow(saturate(1.0f - abs(skyDirection.y)), horizonGlowExponent);

    color += horizonGlowColor * horizonGlow;

    // 太陽の円盤を描く

    float sunDot = saturate(dot(skyDirection, SafeNormalize(lighting.sunDirection)));

    float sunDisk = pow(sunDot, sunDiskExponent);

    float sunGlow = pow(sunDot, sunGlowExponent);

    color += lighting.sunColor * lighting.sunIntensity *
        (sunDisk * sunDiskStrength + sunGlow * sunGlowStrength);

    // HDR色を表示範囲へ圧縮する

    color = ToneMapReinhard(color);

    // 負の成分を除いて出力する

    return float4(max(color, 0.0f), 1.0f);
}
