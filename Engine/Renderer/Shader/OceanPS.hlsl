#include "CommonLighting.hlsli"

// ============================================================
// Constant Buffer
// ============================================================

cbuffer MatrixBuffer : register(b0)
{
    float4x4 mvp;

    float time;
    float3 oceanOffset;

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
SamplerState gSampler : register(s0);

// ============================================================
// Random
// ============================================================

uint HashWaveSeed(uint value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float RandomWave(uint seed)
{
    return float(HashWaveSeed(seed) & 0x00ffffffu) / 16777216.0f;
}

// ============================================================
// Wave Parameters
// ============================================================

static const float TWO_PI = 6.28318530718f;

void GetWaveParameters(
    int i,
    out float2 direction,
    out float wavelength,
    out float amplitudeScale,
    out float steepness,
    out float phaseOffset,
    out float speedScale)
{
    uint seed = (uint) i + 1u;

    float wavelengthRandom = RandomWave(seed * 17u + 11u);
    float directionRandom = RandomWave(seed * 17u + 23u);
    float amplitudeRandom = RandomWave(seed * 17u + 37u);
    float phaseRandom = RandomWave(seed * 17u + 53u);
    float speedRandom = RandomWave(seed * 17u + 71u);
    float directionGroupRandom = RandomWave(seed * 17u + 89u);

    wavelength = exp(
        lerp(log(120.0f), log(1800.0f), wavelengthRandom)
    );

    amplitudeScale = lerp(
        0.30f,
        0.85f,
        amplitudeRandom
    );

    steepness = lerp(
        0.012f,
        0.035f,
        amplitudeRandom
    );

    float directionCenter = directionGroupRandom < 0.60f
        ? 0.0f
        : directionGroupRandom < 0.85f
            ? 1.57079632679f
            : 3.14159265359f;

    float angle = directionCenter
        + (directionRandom - 0.5f) * 1.8f;

    float s = sin(angle);
    float c = cos(angle);

    float2 baseWindDir = normalize(float2(1.0f, 0.6f));

    direction = normalize(mul(
        float2x2(c, -s, s, c),
        baseWindDir
    ));

    phaseOffset = phaseRandom * TWO_PI;

    speedScale = lerp(
        0.85f,
        1.15f,
        speedRandom
    );
}

// ============================================================
// Procedural Wave Normal
// ============================================================

float NoiseValue(int2 cell)
{
    uint hash = HashWaveSeed(asuint(cell.x) + 0x9e3779b9u);
    hash = HashWaveSeed(hash ^ (asuint(cell.y) + 0x85ebca6bu));
    return float(hash & 0x00ffffffu) / 8388608.0f - 1.0f;
}

float ValueNoise2D(float2 position, out float2 gradient)
{
    int2 cell = (int2) floor(position);
    float2 fraction = frac(position);
    float2 blend = fraction * fraction * (3.0f - 2.0f * fraction);
    float2 blendDerivative = 6.0f * fraction * (1.0f - fraction);

    float v00 = NoiseValue(cell);
    float v10 = NoiseValue(cell + int2(1, 0));
    float v01 = NoiseValue(cell + int2(0, 1));
    float v11 = NoiseValue(cell + int2(1, 1));

    float lower = lerp(v00, v10, blend.x);
    float upper = lerp(v01, v11, blend.x);

    gradient.x = lerp(v10 - v00, v11 - v01, blend.y) * blendDerivative.x;
    gradient.y = (upper - lower) * blendDerivative.y;

    return lerp(lower, upper, blend.y);
}

float3 CalculateProceduralWaveNormal(
    float2 worldXZ,
    float distanceToCamera)
{
    float2 slope = float2(0.0f, 0.0f);
    float distanceFade = 1.0f - smoothstep(
        20000.0f,
        120000.0f,
        distanceToCamera
    );

    float2 windDirection = normalize(float2(1.0f, 0.6f));
    float2 crossWindDirection = float2(-windDirection.y, windDirection.x);
    float frequency = 0.045f;
    float heightAmplitude = 1.5f;

    [unroll]
    for (int i = 0; i < 5; ++i)
    {
        float speed = 2.0f + (float) i * 0.8f;
        float2 flowDirection = (i % 2) == 0
            ? windDirection
            : crossWindDirection;
        float2 advectedPosition = worldXZ - flowDirection * time * speed;

        float angle = (float) i * 1.9634954f;
        float cosine = cos(angle);
        float sine = sin(angle);
        float2 rotatedPosition = float2(
            cosine * advectedPosition.x - sine * advectedPosition.y,
            sine * advectedPosition.x + cosine * advectedPosition.y
        );
        float2 noisePosition = rotatedPosition * frequency;

        float2 noiseGradient;
        ValueNoise2D(noisePosition, noiseGradient);

        float2 gradientWorld = float2(
            cosine * noiseGradient.x + sine * noiseGradient.y,
            -sine * noiseGradient.x + cosine * noiseGradient.y
        ) * frequency;

        float footprint = max(
            length(ddx(noisePosition)),
            length(ddy(noisePosition))
        );
        float screenFilter = exp(-0.5f * footprint * footprint);

        float detailFade = lerp(0.2f, 1.0f, distanceFade);
        detailFade = lerp(
            detailFade,
            1.0f,
            saturate(0.12f / frequency)
        );

        slope += gradientWorld
            * heightAmplitude
            * detailFade
            * screenFilter;

        frequency *= 2.0f;
        heightAmplitude *= 0.48f;
    }

    return normalize(float3(
        -slope.x,
        1.0f,
        -slope.y
    ));
}

// ============================================================
// Pixel Shader Input
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
// Pixel Shader
// ============================================================

float4 PS(PS_INPUT input) : SV_TARGET
{
    float3 V = SafeNormalize(cameraPos - input.worldPos);
    float3 L = SafeNormalize(sunDirection);

    float distanceToCamera = length(
        cameraPos - input.worldPos
    );

    // ========================================================
    // Wave Normal
    // ========================================================

    float3 N = SafeNormalize(input.normal);

    float3 detailNormal = CalculateProceduralWaveNormal(
        input.wavePos.xz,
        distanceToCamera
    );

    // 元の頂点法線を維持しながら、細かな波の傾斜を加える
    float3 T = SafeNormalize(input.tangent);
    float3 B = SafeNormalize(cross(N, T));

    float detailStrength = lerp(
        0.25f,
        0.65f,
        1.0f - smoothstep(
            20000.0f,
            100000.0f,
            distanceToCamera
        )
    );

    N = SafeNormalize(
        N
        + T * detailNormal.x * detailStrength
        + B * detailNormal.z * detailStrength
    );

    // ========================================================
    // Lighting Parameters
    // ========================================================

    CommonLightingParameters lighting;

    lighting.sunDirection = L;
    lighting.sunColor = sunColor;
    lighting.sunIntensity = sunIntensity;
    lighting.ambientColor = ambientColor;
    lighting.ambientIntensity = ambientIntensity;

    float NdotV = saturate(dot(N, V));

    // ========================================================
    // Fresnel Reflection
    // ========================================================

    float3 F0 = float3(0.02f, 0.02f, 0.02f);
    const float waterRoughness = 0.14f;

    float3 fresnel = FresnelSchlickRoughness(
        NdotV,
        F0,
        waterRoughness
    );

    float3 R = reflect(-V, N);

    float3 reflectionColor = GetSkyColor(
        R,
        lighting
    );

    // ========================================================
    // Water Color
    // ========================================================

    float3 waterColor = float3(
        0.015f,
        0.065f,
        0.105f
    );

    float3 baseColor = waterColor * 0.35f;

    // ========================================================
    // Sun Specular
    // ========================================================

    float3 sunSpecular = CalculateSunSpecular(
        N,
        V,
        L,
        waterRoughness,
        lighting
    );

    // ========================================================
    // Wave Color Variation
    // ========================================================

    float waveFacing = saturate(dot(N, L));

    float waveHighlight = pow(
        waveFacing,
        8.0f
    );

    float3 waveHighlightColor = float3(
        0.045f,
        0.065f,
        0.075f
    ) * waveHighlight;

    float waveShadow = pow(
        saturate(1.0f - waveFacing),
        1.6f
    );

    float3 waveDarkening = waterColor
        * waveShadow
        * 0.30f;

    // ========================================================
    // Ambient Lighting
    // ========================================================

    float3 ambient = ambientColor
        * ambientIntensity
        * waterColor;

    // ========================================================
    // Subsurface Scattering
    // ========================================================

    float3 subsurfaceColor = float3(
        0.02f,
        0.07f,
        0.09f
    );

    float backLight = saturate(
        dot(-N, L)
    );

    float subsurface = pow(
        backLight,
        2.0f
    );

    float3 subsurfaceLighting = subsurfaceColor
        * subsurface
        * sunIntensity
        * 0.15f;

    // ========================================================
    // Final Lighting
    // ========================================================

    float3 color = baseColor;

    color += reflectionColor * fresnel;
    color += sunSpecular;
    color += ambient;
    color += subsurfaceLighting;
    color += waveHighlightColor;
    color -= waveDarkening;

    // ========================================================
    // Atmospheric Fog
    // ========================================================

    float fogRange = max(
        oceanFogEndDistance - oceanFogStartDistance,
        1.0f
    );

    float fogFactor = saturate(
        (distanceToCamera - oceanFogStartDistance) / fogRange
    );

    fogFactor = fogFactor * fogFactor
        * (3.0f - 2.0f * fogFactor);

    float3 fogColor = GetSkyColor(
        float3(0.0f, 0.3f, 1.0f),
        lighting
    );

    color = lerp(
        color,
        fogColor,
        fogFactor
    );

    // ========================================================
    // Tone Mapping
    // ========================================================

    color = color / (1.0f + color);

    return float4(
        saturate(color),
        1.0f
    );
}
