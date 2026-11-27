#include "CommonLighting.hlsli"

static const float kDistanceFogExtinctionPerMeter = 0.00001f;
static const float kNormalMapStrength = 0.95f;
static const float kSunSpecularStrength = 0.95f;
static const float kOceanFogBlend = 0.5f;
static const float kOceanExposure = 1.2f;
static const float kMinimumOceanNormalUvScale = 1e-6f;
static const float2 kOceanWindDirection = float2(0.85749293f, 0.51449576f);

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
    // ゲルストナー波のうねりと同じ風。Uは風に直交し、Vは風に沿う
    const float2 across = float2(-kOceanWindDirection.y, kOceanWindDirection.x);
    return float2(dot(worldXZ, across), dot(worldXZ, kOceanWindDirection));
}

float2 OceanSurfaceWarp(float2 worldXZ)
{
    const float2 broadPhaseFrequency = float2(0.011f, 0.029f);
    const float2 crossPhaseFrequency = float2(0.019f, 0.013f);
    const float acrossWarpTimeRate = 0.008f;
    const float alongBroadWarpTimeRate = 0.015f;
    const float alongCrossWarpTimeRate = 0.011f;
    const float acrossWarpAmplitude = 0.015f;
    const float alongBroadWarpAmplitude = 0.10f;
    const float alongCrossWarpAmplitude = 0.035f;
    // 位相の変化はほぼ風の進行方向に限り、横方向の歪みは小さく保つ
    // 隣り合う波の群れが、一つの優勢な方向を保つようにするため
    float2 p = OceanFlowCoordinates(worldXZ) * max(oceanNormalUvScale, kMinimumOceanNormalUvScale);
    float broadPhase = p.y * broadPhaseFrequency.y + p.x * broadPhaseFrequency.x;
    float crossPhase = p.y * crossPhaseFrequency.y - p.x * crossPhaseFrequency.x;
    return float2(sin(crossPhase + time * acrossWarpTimeRate) * acrossWarpAmplitude,
        sin(broadPhase + time * alongBroadWarpTimeRate) * alongBroadWarpAmplitude +
        sin(crossPhase - time * alongCrossWarpTimeRate) * alongCrossWarpAmplitude);
}

float3 SampleOceanNormal(float3 wavePos, float2 surfaceWarp)
{
    const float primaryNormalFrequency = 0.018f;
    const float secondaryNormalFrequency = 0.045f;
    const float primaryNormalScrollRate = 0.12f;
    const float secondaryNormalScrollRate = 0.19f;
    const float secondaryWarpScale = 0.73f;
    const float2 secondaryNormalOffset = float2(17.13f, 5.71f);
    const float slopeWarpStrength = 0.15f;
    float uvScale = max(oceanNormalUvScale, kMinimumOceanNormalUvScale);

    float2 flowPosition = OceanFlowCoordinates(wavePos.xz);
    float2 uv1 = (flowPosition * primaryNormalFrequency + float2(0, -time * primaryNormalScrollRate)) * uvScale;
    float2 uv2 = (flowPosition * secondaryNormalFrequency + float2(0, -time * secondaryNormalScrollRate)) * uvScale;
    uv1 += surfaceWarp;
    uv2 += surfaceWarp * secondaryWarpScale + secondaryNormalOffset;

    float3 n1 = gOceanNormal.Sample(gSampler, uv1).xyz * 2.0f - 1.0f;
    float3 n2 = gOceanNormal2.Sample(gSampler, uv2).xyz * 2.0f - 1.0f;
    float2 flowSlopes = n1.xy * (1.0f + surfaceWarp.y * slopeWarpStrength) +
        n2.xy * (1.0f - surfaceWarp.x * slopeWarpStrength);
    // マップの傾きを、風の座標系からT(+X)、B(-Z)へ変換する
    const float2 wind = kOceanWindDirection;
    float2 worldSlopes = float2(-wind.y, wind.x) * flowSlopes.x + wind * flowSlopes.y;
    float2 slopes = float2(worldSlopes.x, -worldSlopes.y);
    float3 normal = float3(slopes, n1.z * n2.z);
    normal.xy *= kNormalMapStrength;

    return SafeNormalize(normal);
}

// 海面の色を計算する

float4 PS(PS_INPUT input) : SV_TARGET
{
    const float3 waterReflectance = float3(0.025f, 0.025f, 0.025f);
    const float waterFresnelRoughness = 0.24f;
    const float skyReflectionStrength = 0.30f;
    const float3 waterColor = float3(0.045f, 0.14f, 0.25f);
    const float waterBaseStrength = 0.85f;
    const float waterBaseUnlitWeight = 0.65f;
    const float waterBaseSunWeight = 0.45f;
    const float resolvedNormalRoughness = 0.16f;
    const float unresolvedNormalRoughness = 0.32f;
    const float normalVariationFadeStart = 0.01f;
    const float normalVariationFadeEnd = 0.08f;
    const float skyLightMin = 0.85f;
    const float skyLightMax = 1.15f;
    const float waterAmbientStrength = 1.25f;
    const float3 waterScatteringColor = float3(0.015f, 0.055f, 0.12f);
    const float waterScatteringStrength = 0.35f;
    const float wavePatternFrequency = 0.025f;
    const float wavePatternScrollRate = 0.40f;
    const float wavePatternWarpScale = 0.61f;
    const float wavePatternDarkThreshold = 0.30f;
    const float wavePatternBrightThreshold = 0.70f;
    const float waveColorStrength = 0.25f;
    const float horizonFogExponent = 4.0f;
    const float horizonFogStrength = 0.2f;
    const float3 oceanFogTint = float3(0.82f, 0.90f, 0.96f);
    const float oceanFogTintWeight = 0.35f;
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

    float3 F = FresnelSchlickRoughness(NdotV, waterReflectance, waterFresnelRoughness);

    // 空の反射

    float3 R = reflect(-V, N);
    float3 reflectedSky = GetSkyColor(R, lighting);
    float3 reflection = reflectedSky * F * skyReflectionStrength;

    // 水面の基本色

    float3 waterBase = waterColor * waterBaseStrength * (waterBaseUnlitWeight + waterBaseSunWeight * NdotL);

    // 海面の鏡面反射

    // 法線の変化がサブピクセルのときは、ちらつきを出さずにハイライトを広げる
    float normalVariation = length(fwidth(N));
    float roughness = lerp(resolvedNormalRoughness, unresolvedNormalRoughness,
        smoothstep(normalVariationFadeStart, normalVariationFadeEnd, normalVariation));
    float3 sunSpecular = CalculateSunSpecular(N, V, L, roughness, lighting);

    sunSpecular *= kSunSpecularStrength;

    // 環境光

    float skyLight = lerp(skyLightMin, skyLightMax, saturate(N.y));

    float3 ambient = waterColor * GetAmbientRadiance(lighting) * skyLight * waterAmbientStrength;

    // 水中の散乱光

    float backLight = saturate(dot(-L, N));

    float3 subsurface = waterScatteringColor * pow(backLight, 2.0f) * waterScatteringStrength;

    // 波の高さに応じた明るさ

    float wavePattern =
        gDiffuse
            .Sample(gSampler,
                (OceanFlowCoordinates(input.wavePos.xz) * wavePatternFrequency +
                    float2(0, -time * wavePatternScrollRate)) * oceanWavePatternUvScale +
                    surfaceWarp * wavePatternWarpScale)
            .r;

    wavePattern = smoothstep(wavePatternDarkThreshold, wavePatternBrightThreshold, wavePattern);

    float3 waveColor = waterColor * wavePattern * waveColorStrength;

    // 海面の照明を合成する

    float3 color = waterBase + reflection + sunSpecular + ambient + subsurface + waveColor;

    // 距離に応じた大気の霧

    float cameraDistance = length(cameraPos - input.worldPos);

    float fogFactor = 1.0f - exp(-cameraDistance * kDistanceFogExtinctionPerMeter);
    fogFactor = saturate(fogFactor);

    float horizon = pow(1.0f - saturate(abs(V.y)), horizonFogExponent);
    fogFactor = max(fogFactor, horizon * horizonFogStrength);

    float3 atmosphere = GetSkyColor(-V, lighting);
    atmosphere = lerp(atmosphere, oceanFogTint, oceanFogTintWeight);

    fogFactor *= kOceanFogBlend;
    color = lerp(color, atmosphere, fogFactor);

    // HDR色を表示範囲へ圧縮する

    color *= kOceanExposure;
    color = ToneMapReinhard(color);

    return float4(saturate(color), 0.0f);
}
