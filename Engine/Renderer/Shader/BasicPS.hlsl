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

Texture2D gDiffuse : register(t0);
SamplerState gSampler : register(s0);

// ============================================================
// Input
// ============================================================

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;

    float3 worldPos : TEXCOORD1;
    float3 wavePos : TEXCOORD2;

    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

// ============================================================
// Main
// ============================================================

float4 PS(PS_INPUT input) : SV_TARGET
{
    // --------------------------------------------------------
    // View / Light Direction
    // --------------------------------------------------------

    float3 V = SafeNormalize(cameraPos - input.worldPos);
    float3 L = SafeNormalize(sunDirection);

    // --------------------------------------------------------
    // Normal
    // --------------------------------------------------------

    float3 N;
    float3 T;
    float3 B;

    BuildTBN(input.normal, input.tangent, N, T, B);

    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));

    // --------------------------------------------------------
    // Diffuse
    // --------------------------------------------------------

    float3 albedo = gDiffuse.Sample(gSampler, input.texcoord).rgb;
    float3 diffuse = albedo * NdotL;

    // --------------------------------------------------------
    // Ambient
    // --------------------------------------------------------

    float3 ambient = albedo * ambientColor * ambientIntensity;

    // --------------------------------------------------------
    // Sun Specular
    // --------------------------------------------------------

    float3 sunSpecular = CalculateSunSpecular(
        N,
        V,
        L,
        0.35f,
        sunColor,
        sunIntensity
    );

    // --------------------------------------------------------
    // Final Lighting
    // --------------------------------------------------------

    float3 color = diffuse + ambient + sunSpecular;

    // --------------------------------------------------------
    // Atmospheric Fog
    // --------------------------------------------------------

    float distanceToCamera = length(cameraPos - input.worldPos);

    float fogStart = 1500.0f;
    float fogEnd = 12000.0f;

    float fogFactor = saturate(
        (distanceToCamera - fogStart) /
        (fogEnd - fogStart)
    );

    float3 atmosphere = GetSkyColor(
        V,
        sunDirection,
        sunColor,
        sunIntensity
    );

    color = lerp(color, atmosphere, fogFactor);

    // --------------------------------------------------------
    // Tonemapping
    // --------------------------------------------------------

    color = color / (1.0f + color);

    return float4(saturate(color), 1.0f);
}