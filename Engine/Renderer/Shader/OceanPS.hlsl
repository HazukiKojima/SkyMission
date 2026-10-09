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
    float oceanUvReferenceSize;
    float oceanFogStartRatio;
    float oceanFogEndRatio;
    float oceanWaveScale;
};

// ============================================================
// Resources
// ============================================================

Texture2D gDiffuse : register(t0);
Texture2D gOceanNormal : register(t1);
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
    float detailScale = min(oceanWaveScale, 25.0f);
    float uvScale = 1.0f / max(detailScale, 1e-4f);

    float2 uv1 = (wavePos.xz * 0.010f + float2(time * 0.12f, time * -0.08f)) * uvScale;
    float2 uv2 = (wavePos.xz * 0.035f + float2(time * -0.25f, time * 0.18f)) * uvScale;
    float2 uv3 = (wavePos.xz * 0.090f + float2(time * 0.45f, time * 0.32f)) * uvScale;

    float3 n1 = gOceanNormal.Sample(gSampler, uv1).xyz * 2.0f - 1.0f;
    float3 n2 = gOceanNormal.Sample(gSampler, uv2).xyz * 2.0f - 1.0f;
    float3 n3 = gOceanNormal.Sample(gSampler, uv3).xyz * 2.0f - 1.0f;

    float3 normal = n1 * 0.55f + n2 * 0.30f + n3 * 0.15f;
    normal.xy *= 0.75f;

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

    float3 F0 = float3(0.020f, 0.020f, 0.020f);

    float3 F = FresnelSchlickRoughness(NdotV, F0, 0.08f);
    F = max(F, 0.035f);

    // --------------------------------------------------------
    // Reflection
    // --------------------------------------------------------

    float3 R = reflect(-V, N);
    float3 reflectedSky = GetSkyColor(R, lighting);
    float3 reflection = reflectedSky * F;

    // --------------------------------------------------------
    // Water Color
    // --------------------------------------------------------

    float3 waterColor = float3(0.015f, 0.075f, 0.12f);
    float3 waterBase = waterColor * 0.35f;

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

    // --------------------------------------------------------
    // Ambient
    // --------------------------------------------------------

    float skyLight = lerp(0.75f, 1.0f, saturate(N.y));

    float3 ambient =
        waterColor *
        GetAmbientRadiance(lighting) *
        skyLight;

    // --------------------------------------------------------
    // Subsurface
    // --------------------------------------------------------

    float backLight = saturate(dot(-L, N));

    float3 subsurface =
        float3(0.00f, 0.12f, 0.18f) *
        pow(backLight, 2.0f) *
        0.35f;

    // --------------------------------------------------------
    // Wave Brightness
    // --------------------------------------------------------

    float detailScale = min(oceanWaveScale, 25.0f);
    float uvScale = 1.0f / max(detailScale, 1e-4f);

    float wavePattern = gDiffuse.Sample(
        gSampler,
        (input.wavePos.xz * 0.025f + float2(time * 0.40f, -time * 0.30f)) * uvScale
    ).r;

    wavePattern = smoothstep(0.30f, 0.70f, wavePattern);

    float3 waveColor = waterColor * wavePattern * 0.25f;

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

    float distanceToCamera = length(cameraPos - input.worldPos);

    float fogStart = oceanSize * oceanFogStartRatio;
    float fogEnd = max(oceanSize * oceanFogEndRatio, fogStart + 1.0f);

    float fogFactor = saturate(
        (distanceToCamera - fogStart) /
        (fogEnd - fogStart)
    );

    float2 oceanLocalPosition = input.wavePos.xz - cameraPos.xz;
    float oceanEdgeDistance = max(
        abs(oceanLocalPosition.x),
        abs(oceanLocalPosition.y)
    ) / max(oceanSize * 0.5f, 1.0f);
    float edgeFade = smoothstep(0.98f, 0.995f, oceanEdgeDistance);
    fogFactor = max(fogFactor, edgeFade);

    float horizon = pow(
        1.0f - saturate(abs(V.y)),
        4.0f
    );

    fogFactor = max(fogFactor, horizon * 0.2f);

    float3 atmosphere = GetSkyColor(-V, lighting);

    color = lerp(color, atmosphere, fogFactor);

    // --------------------------------------------------------
    // Tonemapping
    // --------------------------------------------------------

    //color = ToneMapReinhard(color);

    //return float4(saturate(color), 1.0f);
    
    color = ToneMapReinhard(color);

    return float4(saturate(color), 0.0f);
}