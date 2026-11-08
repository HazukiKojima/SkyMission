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

// 参照するテクスチャとサンプラー

Texture2D gDiffuse : register(t0);
Texture2D gOceanNormal : register(t1);
Texture2D gOceanNormal2 : register(t2);
SamplerState gSampler : register(s0);

// 頂点シェーダーから受け取る値

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float3 wavePos : TEXCOORD2;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

// 海面の法線を計算する

float2 OceanFlowCoordinates(float2 worldXZ)
{
    // Same wind as the Gerstner swell: U crosses the wind, V follows it.
    const float2 wind = float2(0.85749293f, 0.51449576f);
    const float2 across = float2(-wind.y, wind.x);
    return float2(dot(worldXZ, across), dot(worldXZ, wind));
}

float2 OceanSurfaceWarp(float2 worldXZ)
{
    // Mostly longitudinal phase variation; keep lateral distortion small so
    // neighbouring wave groups retain one prevailing direction.
    float2 p = OceanFlowCoordinates(worldXZ) * max(oceanNormalUvScale, 1e-6f);
    float broadPhase = p.y * 0.029f + p.x * 0.011f;
    float crossPhase = p.y * 0.013f - p.x * 0.019f;
    return float2(sin(crossPhase + time * 0.008f) * 0.015f,
        sin(broadPhase + time * 0.015f) * 0.10f + sin(crossPhase - time * 0.011f) * 0.035f);
}

float3 SampleOceanNormal(float3 wavePos, float2 surfaceWarp)
{
    float uvScale = max(oceanNormalUvScale, 1e-6f);

    float2 flowPosition = OceanFlowCoordinates(wavePos.xz);
    float2 uv1 = (flowPosition * 0.018f + float2(0, -time * 0.12f)) * uvScale;
    float2 uv2 = (flowPosition * 0.045f + float2(0, -time * 0.19f)) * uvScale;
    uv1 += surfaceWarp;
    uv2 += surfaceWarp * 0.73f + float2(17.13f, 5.71f);

    float3 n1 = gOceanNormal.Sample(gSampler, uv1).xyz * 2.0f - 1.0f;
    float3 n2 = gOceanNormal2.Sample(gSampler, uv2).xyz * 2.0f - 1.0f;
    float2 flowSlopes = n1.xy * (1.0f + surfaceWarp.y * 0.15f) +
        n2.xy * (1.0f - surfaceWarp.x * 0.15f);
    // Convert map slopes from the wind frame to T (+X), B (-Z).
    const float2 wind = float2(0.85749293f, 0.51449576f);
    float2 worldSlopes = float2(-wind.y, wind.x) * flowSlopes.x + wind * flowSlopes.y;
    float2 slopes = float2(worldSlopes.x, -worldSlopes.y);
    float3 normal = float3(slopes, n1.z * n2.z);
    normal.xy *= 0.95f;

    return SafeNormalize(normal);
}

// 海面の色を計算する

float4 PS(PS_INPUT input) : SV_TARGET
{
    CommonLightingParameters lighting;
    lighting.sunDirection = sunDirection;
    lighting.sunIntensity = sunIntensity;
    lighting.sunColor = sunColor;
    lighting.ambientIntensity = ambientIntensity;
    lighting.ambientColor = ambientColor;
    lighting.padding = 0.0f;

    // 視線方向と光源方向

    float3 V = SafeNormalize(cameraPos - input.worldPos);
    float3 L = SafeNormalize(lighting.sunDirection);

    // 海面の接線空間の基底

    float3 baseN;
    float3 T;
    float3 B;

    BuildTBN(input.normal, input.tangent, baseN, T, B);

    // 海面の法線を計算する

    float2 surfaceWarp = OceanSurfaceWarp(input.wavePos.xz);
    float3 tangentNormal = SampleOceanNormal(input.wavePos, surfaceWarp);

    float3 N = SafeNormalize(T * tangentNormal.x + B * tangentNormal.y + baseN * tangentNormal.z);

    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));

    // 水面の材質

    float3 F = FresnelSchlickRoughness(NdotV, float3(0.025f, 0.025f, 0.025f), 0.24f);

    // 空の反射

    float3 R = reflect(-V, N);
    float3 reflectedSky = GetSkyColor(R, lighting);
    float3 reflection = reflectedSky * F * 0.30f;

    // 水面の基本色

    float3 waterColor = float3(0.045f, 0.14f, 0.25f);
    float3 waterBase = waterColor * 0.85f * (0.65f + 0.45f * NdotL);

    // 海面の鏡面反射

    // Broaden highlights when normal variation is subpixel instead of producing glitter.
    float normalVariation = length(fwidth(N));
    float roughness = lerp(0.16f, 0.32f, smoothstep(0.01f, 0.08f, normalVariation));
    float3 sunSpecular = CalculateSunSpecular(N, V, L, roughness, lighting);

    sunSpecular *= 0.95f;

    // 環境光

    float skyLight = lerp(0.85f, 1.15f, saturate(N.y));

    float3 ambient = waterColor * GetAmbientRadiance(lighting) * skyLight * 1.25f;

    // 水中の散乱光

    float backLight = saturate(dot(-L, N));

    float3 subsurface = float3(0.015f, 0.055f, 0.12f) * pow(backLight, 2.0f) * 0.35f;

    // 波の高さに応じた明るさ

    float wavePattern =
        gDiffuse
            .Sample(gSampler,
                (OceanFlowCoordinates(input.wavePos.xz) * 0.025f + float2(0, -time * 0.40f)) * oceanWavePatternUvScale +
                    surfaceWarp * 0.61f)
            .r;

    wavePattern = smoothstep(0.30f, 0.70f, wavePattern);

    float3 waveColor = waterColor * wavePattern * 0.25f;

    // 海面の照明を合成する

    float3 color = waterBase + reflection + sunSpecular + ambient + subsurface + waveColor;

    // 距離に応じた大気の霧

    float cameraDistance = length(cameraPos - input.worldPos);

    float fogFactor = 1.0f - exp(-cameraDistance * 0.00001f);
    fogFactor = saturate(fogFactor);

    float horizon = pow(1.0f - saturate(abs(V.y)), 4.0f);
    fogFactor = max(fogFactor, horizon * 0.2f);

    float3 atmosphere = GetSkyColor(-V, lighting);
    atmosphere = lerp(atmosphere, float3(0.82f, 0.90f, 0.96f), 0.35f);

    fogFactor *= 0.5f;
    color = lerp(color, atmosphere, fogFactor);

    // HDR色を表示範囲へ圧縮する

    color *= 1.2f;
    color = ToneMapReinhard(color);

    return float4(saturate(color), 0.0f);
}
