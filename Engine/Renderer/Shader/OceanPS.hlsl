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

    float oceanSize;
    float oceanWaveLength;
    float oceanWaveSteepness;
    float oceanNormalUvScale;
    float oceanWavePatternUvScale;
    float oceanFogStartDistance;
    float oceanFogEndDistance;
    float oceanEdgeFadeStart;
    float oceanEdgeFadeWidth;
};

// ============================================================
// Resources
// ============================================================

Texture2D gDiffuse : register(t0);
Texture2D gOceanNormal : register(t1);
Texture2D gOceanNormal2 : register(t2);
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
// Ocean Normal
// ============================================================

float3 SampleOceanNormal(float3 wavePos)
{
    float uvScale = max(oceanNormalUvScale, 1e-6f);

    float2 uv1 = (wavePos.xz * 0.018f + float2(time * 0.12f, time * -0.08f)) * uvScale;
    float2 uv2 = (wavePos.xz * 0.045f + float2(time * -0.19f, time * 0.14f)) * uvScale;

    float3 n1 = gOceanNormal.Sample(gSampler, uv1).xyz * 2.0f - 1.0f;
    float3 n2 = gOceanNormal2.Sample(gSampler, uv2).xyz * 2.0f - 1.0f;

    float3 normal = float3(n1.xy + n2.xy, n1.z * n2.z);
    normal.xy *= 1.00f;

    return SafeNormalize(normal);
}

// ============================================================
// Main
// ============================================================

float4 PS(PS_INPUT input) : SV_TARGET
{
    CommonLightingParameters lighting;
    lighting.sunDirection = sunDirection;
    lighting.sunIntensity = sunIntensity;
    lighting.sunColor = sunColor;
    lighting.ambientIntensity = ambientIntensity;
    lighting.ambientColor = ambientColor;
    lighting.padding = 0.0f;

    // --------------------------------------------------------
    // View / Light Direction
    // --------------------------------------------------------

    float3 V = SafeNormalize(cameraPos - input.worldPos);
    float3 L = SafeNormalize(lighting.sunDirection);

    // --------------------------------------------------------
    // Base TBN
    // --------------------------------------------------------

    float3 baseN;
    float3 T;
    float3 B;

    BuildTBN(input.normal, input.tangent, baseN, T, B);

    // --------------------------------------------------------
    // Ocean Normal
    // --------------------------------------------------------

    float3 tangentNormal = SampleOceanNormal(input.wavePos);

    float3 N = SafeNormalize(
        T * tangentNormal.x +
        B * tangentNormal.y +
        baseN * tangentNormal.z
    );

    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));

    // --------------------------------------------------------
    // Water Material
    // --------------------------------------------------------

    float3 F = float3(0.06f, 0.06f, 0.06f);

    // --------------------------------------------------------
    // Reflection
    // --------------------------------------------------------

    float3 R = reflect(-V, N);
    float3 reflectedSky = GetSkyColor(R, lighting);
    float3 reflection = reflectedSky * F;

    // --------------------------------------------------------
    // Water Color
    // --------------------------------------------------------

    float3 waterColor = float3(0.045f, 0.14f, 0.25f);
    float3 waterBase = waterColor * 0.85f;

    // --------------------------------------------------------
    // Ocean Specular
    // --------------------------------------------------------

    float3 sunSpecular = CalculateSunSpecular(
    N,
    V,
    L,
    0.08f,
    lighting
);

    sunSpecular *= 1.35f;

    // --------------------------------------------------------
    // Ambient
    // --------------------------------------------------------

    float skyLight = lerp(0.85f, 1.15f, saturate(N.y));

    float3 ambient =
        waterColor *
        GetAmbientRadiance(lighting) *
        skyLight *
        1.25f;

    // --------------------------------------------------------
    // Subsurface
    // --------------------------------------------------------

    float backLight = saturate(dot(-L, N));

    float3 subsurface =
        float3(0.015f, 0.055f, 0.12f) *
        pow(backLight, 2.0f) *
        0.35f;

    // --------------------------------------------------------
    // Wave Brightness
    // --------------------------------------------------------

    float wavePattern = gDiffuse.Sample(
        gSampler,
        (input.wavePos.xz * 0.025f +
         float2(time * 0.40f, -time * 0.30f)) *
        oceanWavePatternUvScale
    ).r;

    wavePattern = smoothstep(0.30f, 0.70f, wavePattern);

    float3 waveColor = waterColor * wavePattern * 0.35f;

        // --------------------------------------------------------
    // Final Water
    // --------------------------------------------------------

    float3 color =
        waterBase +
        reflection +
        sunSpecular +
        ambient +
        subsurface +
        waveColor;

    // --------------------------------------------------------
    // Atmospheric Fog
    // --------------------------------------------------------

    float cameraDistance = length(cameraPos - input.worldPos);

    float fogFactor = 1.0f - exp(-cameraDistance * 0.00001f);
    fogFactor = saturate(fogFactor);

    float horizon = pow(1.0f - saturate(abs(V.y)), 4.0f);
    fogFactor = max(fogFactor, horizon * 0.2f);

    float3 atmosphere = GetSkyColor(-V, lighting);
    atmosphere = lerp(atmosphere, float3(0.82f, 0.90f, 0.96f), 0.35f);

    fogFactor *= 0.5f;
    color = lerp(color, atmosphere, fogFactor);

    // --------------------------------------------------------
    // Tonemapping
    // --------------------------------------------------------

    color *= 1.2f;
    color = ToneMapReinhard(color);

    return float4(saturate(color), 0.0f);
}