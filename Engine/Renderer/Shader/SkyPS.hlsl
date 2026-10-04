#include "CommonLighting.hlsli"

// ============================================================
// Constant Buffer
// ============================================================

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

// ============================================================
// Resources
// ============================================================

Texture2D gSkyTexture : register(t0);
SamplerState gSampler : register(s0);

// ============================================================
// Input
// ============================================================

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
};

// ============================================================
// Pixel Shader
// ============================================================

float4 main(PS_INPUT input) : SV_TARGET
{
    // ========================================================
    // Lighting
    // ========================================================

    CommonLightingParameters lighting;

    lighting.sunDirection = sunDirection;
    lighting.sunIntensity = sunIntensity;
    lighting.sunColor = sunColor;
    lighting.ambientIntensity = ambientIntensity;
    lighting.ambientColor = ambientColor;
    lighting.padding = 0.0f;

    // ========================================================
    // HDR Sky
    // ========================================================

    float3 hdrColor = gSkyTexture.Sample(
        gSampler,
        input.texcoord
    ).rgb;

    // ========================================================
    // Exposure
    // ========================================================

    const float exposure = 0.65f;

    float3 color = hdrColor * exposure;

    // ========================================================
    // Atmospheric Horizon
    // ========================================================

    float3 skyDirection = SafeNormalize(
        input.worldPos - cameraPos
    );

    float height = saturate(skyDirection.y);

    // 地平線付近だけ少し大気色を混ぜる
    float horizonFactor = pow(
        1.0f - height,
        3.0f
    );

    float3 atmosphericColor = GetSkyColor(
        skyDirection,
        lighting
    );

    color = lerp(
        color,
        color * 0.72f + atmosphericColor * 0.28f,
        horizonFactor
    );

    // ========================================================
    // Horizon Brightening
    // ========================================================

    float horizonGlow = pow(
        saturate(
            1.0f - abs(skyDirection.y)
        ),
        7.0f
    );

    color += float3(
        0.025f,
        0.035f,
        0.045f
    ) * horizonGlow;

    // ========================================================
    // Sun Disk
    // ========================================================

    float sunDot = saturate(
        dot(
            skyDirection,
            SafeNormalize(lighting.sunDirection)
        )
    );

    float sunDisk = pow(
        sunDot,
        18000.0f
    );

    float sunGlow = pow(
        sunDot,
        180.0f
    );

    color +=
        lighting.sunColor *
        lighting.sunIntensity *
        (
            sunDisk * 0.65f +
            sunGlow * 0.035f
        );

    // ========================================================
    // Tone Mapping
    // ========================================================

    color = ToneMapReinhard(color);

    // ========================================================
    // Gamma / Final
    // ========================================================

    return float4(
        max(color, 0.0f),
        1.0f
    );
}