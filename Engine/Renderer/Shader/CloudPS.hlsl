#include "CommonLighting.hlsli"

// ============================================================
// Constant Buffer
// ============================================================

cbuffer CloudConstants : register(b0)
{
    float4x4 inverseViewProjection;
    float3 cameraPosition;
    float time;
    float3 sunDirection;
    float sunIntensity;
    float3 sunColor;
    float cloudDensity;
    float cloudBottom;
    float cloudTop;
    float shapeScale;
    float detailScale;
    float detailStrength;
    float absorption;
    float stepSize;
    int stepCount;
    float ambientIntensity;
    float3 ambientColor;
};

// ============================================================
// Resources
// ============================================================

Texture2D gSceneColor : register(t0);
Texture2D gSceneDepth : register(t1);
SamplerState gSampler : register(s0);

// ============================================================
// Pixel Input
// ============================================================

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

// ============================================================
// Cloud Volume
// ============================================================

static const float3 CLOUD_MIN = float3(-12000.0f, 800.0f, -12000.0f);
static const float3 CLOUD_MAX = float3(12000.0f, 1800.0f, 12000.0f);

// ============================================================
// SkyGL-inspired tuning
// ============================================================

// 雲量。
// 小さいほど青空が増える。
static const float CLOUD_COVERAGE = 0.32f;

// 雲の形状コントラスト。
static const float CLOUD_CONTRAST = 1.10f;

// ============================================================
// Hash
// ============================================================

float Hash31(float3 p)
{
    p = frac(p * 0.1031f);
    p += dot(p, p.yzx + 33.33f);
    return frac((p.x + p.y) * p.z);
}

// ============================================================
// Value Noise
// ============================================================

float ValueNoise3D(float3 p)
{
    float3 cell = floor(p);
    float3 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);

    float n000 = Hash31(cell);
    float n100 = Hash31(cell + float3(1.0f, 0.0f, 0.0f));
    float n010 = Hash31(cell + float3(0.0f, 1.0f, 0.0f));
    float n110 = Hash31(cell + float3(1.0f, 1.0f, 0.0f));
    float n001 = Hash31(cell + float3(0.0f, 0.0f, 1.0f));
    float n101 = Hash31(cell + float3(1.0f, 0.0f, 1.0f));
    float n011 = Hash31(cell + float3(0.0f, 1.0f, 1.0f));
    float n111 = Hash31(cell + float3(1.0f, 1.0f, 1.0f));

    float nx00 = lerp(n000, n100, f.x);
    float nx10 = lerp(n010, n110, f.x);
    float nx01 = lerp(n001, n101, f.x);
    float nx11 = lerp(n011, n111, f.x);
    float nxy0 = lerp(nx00, nx10, f.y);
    float nxy1 = lerp(nx01, nx11, f.y);

    return lerp(nxy0, nxy1, f.z);
}

// ============================================================
// FBM
// ============================================================

float CloudFBM(float3 p)
{
    float value = 0.0f;

    // Large shape
    value += ValueNoise3D(p) * 0.60f;

    // Medium shape
    p = p * 2.01f + float3(17.2f, 4.3f, 11.7f);
    value += ValueNoise3D(p) * 0.28f;

    // Fine shape
    p = p * 2.03f + float3(5.1f, 13.7f, 8.4f);
    value += ValueNoise3D(p) * 0.12f;

    return saturate(value);
}

// ============================================================
// Height Profile
// ============================================================

float CloudHeightProfile(float h)
{
    h = saturate(h);

    float bottom = smoothstep(0.02f, 0.20f, h);
    float top = 1.0f - smoothstep(0.68f, 1.0f, h);

    return bottom * top;
}

float CloudCellRandom(float2 cell, float seed)
{
    return Hash31(float3(cell, seed));
}

// ============================================================
// Density
// ============================================================

float CloudDensityAt(float3 worldPosition, bool detail)
{
    float layerHeight = max(cloudTop - cloudBottom, 1.0f);
    const float cellSize = 5200.0f;
    float2 cloudGrid = worldPosition.xz / cellSize;
    float2 baseCell = floor(cloudGrid);
    float2 neighborDirection = lerp(
        float2(-1.0f, -1.0f),
        float2(1.0f, 1.0f),
        step(float2(0.5f, 0.5f), frac(cloudGrid)));
    float placementMask = 0.0f;

    [unroll]
    for (int candidate = 0; candidate < 4; ++candidate)
    {
        float2 candidateOffset = float2(0.0f, 0.0f);
        if (candidate == 1)
            candidateOffset.x = neighborDirection.x;
        else if (candidate == 2)
            candidateOffset.y = neighborDirection.y;
        else if (candidate == 3)
            candidateOffset = neighborDirection;

        float2 cell = baseCell + candidateOffset;
        float spawn = CloudCellRandom(cell, 17.13f);

        if (spawn < 0.55f)
            continue;

        float2 center = cell + float2(
            0.18f + CloudCellRandom(cell, 31.71f) * 0.64f,
            0.18f + CloudCellRandom(cell, 47.29f) * 0.64f);
        float2 radius = float2(
            0.22f + CloudCellRandom(cell, 61.83f) * 0.28f,
            0.20f + CloudCellRandom(cell, 79.37f) * 0.30f);
        float2 localPosition = (cloudGrid - center) / radius;
        float radialDistanceSquared = dot(localPosition, localPosition);

        float cloudCenterHeight = lerp(
            cloudBottom + layerHeight * 0.18f,
            cloudTop - layerHeight * 0.18f,
            CloudCellRandom(cell, 93.11f));
        float cloudThickness = layerHeight * lerp(
            0.28f,
            0.58f,
            CloudCellRandom(cell, 107.53f));
        float localHeight = (worldPosition.y - cloudCenterHeight) / cloudThickness + 0.5f;
        float heightMask = CloudHeightProfile(localHeight);

        float edgeVariation = CloudCellRandom(cell, 131.17f) * 0.08f;
        float footprint = 1.0f - smoothstep(
            0.49f + edgeVariation,
            1.1025f + edgeVariation,
            radialDistanceSquared);

        placementMask = max(placementMask, footprint * heightMask);
    }

    if (placementMask <= 0.001f)
        return 0.0f;

    // ========================================================
    // Wind
    // ========================================================

    float3 wind = SafeNormalize(float3(0.75f, 0.0f, 0.35f));
    float3 windOffset = wind * time * 10.0f;

    // ========================================================
    // Large Cloud Shape
    // ========================================================

    float3 shapePosition = (worldPosition + windOffset) * shapeScale;
    shapePosition.y *= 0.72f;

    float domainWarp = ValueNoise3D(
        shapePosition * 0.55f +
        float3(7.1f, 2.8f, 13.4f)) - 0.5f;

    shapePosition.x += domainWarp * 0.65f;
    shapePosition.z -= domainWarp * 0.45f;

    float shape = CloudFBM(shapePosition);

    // ========================================================
    // Sparse Coverage
    // ========================================================

    float density = smoothstep(
        1.0f - CLOUD_COVERAGE - 0.10f,
        1.0f - CLOUD_COVERAGE + 0.04f,
        shape);

    density = pow(saturate(density), CLOUD_CONTRAST);

    // ========================================================
    // Fine Erosion
    // ========================================================

    if (detail)
    {
        float3 detailPosition = (
            worldPosition +
            windOffset * 1.6f
        ) * detailScale;

        detailPosition.y *= 1.15f;

        float detailNoise = ValueNoise3D(detailPosition);

        density -= (
            1.0f -
            detailNoise
        ) * detailStrength * density;
    }

    // ========================================================
    // Height
    // ========================================================

    density *= placementMask;
    density *= cloudDensity;

    return saturate(density);
}

// ============================================================
// Ray Box
// ============================================================

bool RayBox(
    float3 origin,
    float3 direction,
    float3 boxMin,
    float3 boxMax,
    out float tMin,
    out float tMax)
{
    float3 invDirection = 1.0f / max(abs(direction), 1e-5f);
    invDirection *= sign(direction);

    float3 t0 = (boxMin - origin) * invDirection;
    float3 t1 = (boxMax - origin) * invDirection;

    float3 tSmall = min(t0, t1);
    float3 tLarge = max(t0, t1);

    tMin = max(max(tSmall.x, tSmall.y), tSmall.z);
    tMax = min(min(tLarge.x, tLarge.y), tLarge.z);

    return tMax > max(tMin, 0.0f);
}

// ============================================================
// World Position
// ============================================================

float3 ReconstructWorldPosition(float2 uv, float depth)
{
    float2 ndc = uv * 2.0f - 1.0f;
    ndc.y = -ndc.y;

    float4 clip = float4(ndc, depth, 1.0f);
    float4 world = mul(clip, inverseViewProjection);

    return world.xyz / max(world.w, 1e-5f);
}

// ============================================================
// Sun Light March
// ============================================================

float SampleCloudSunLight(float3 position)
{
    float3 lightDirection = SafeNormalize(sunDirection);
    const float LIGHT_STEP = 140.0f;

    float density0 = CloudDensityAt(
        position + lightDirection * LIGHT_STEP,
        false);

    float density1 = CloudDensityAt(
        position + lightDirection * LIGHT_STEP * 2.0f,
        false);

    float density2 = CloudDensityAt(
        position + lightDirection * LIGHT_STEP * 3.0f,
        false);

    float opticalDepth =
        density0 * 0.55f +
        density1 * 0.30f +
        density2 * 0.15f;

    // 光の減衰を計算する。
    float multipleScatter = CloudMultipleScattering(opticalDepth);

    return multipleScatter;
}

// ============================================================
// Sky Light
// ============================================================

float3 SampleCloudSkyLight(
    float3 position,
    CommonLightingParameters lighting)
{
    float height = saturate(
        (position.y - cloudBottom) /
        max(cloudTop - cloudBottom, 1.0f));

    float3 sky = GetSkyColor(
        float3(
            0.0f,
            0.65f + height * 0.35f,
            0.0f),
        lighting);

    // 雲内部では空光を少し弱める
    return sky * lerp(0.55f, 0.90f, height);
}

// ============================================================
// Cloud Lighting
// ============================================================

float3 EvaluateCloudLighting(
    float3 position,
    float density,
    float3 viewDirection,
    CommonLightingParameters lighting)
{
    float3 lightDirection = SafeNormalize(lighting.sunDirection);

    // ========================================================
    // Dual Lobe HG
    // ========================================================

    float cosTheta = dot(viewDirection, lightDirection);
    float phase = DualLobeCloudPhase(cosTheta);

    // ========================================================
    // Sun Light
    // ========================================================

    float sunVisibility = SampleCloudSunLight(position);

    float3 sunLight = GetSunRadiance(lighting) * sunVisibility;
    sunLight *= (0.35f + phase * 0.65f);

    // ========================================================
    // Sky Light
    // ========================================================

    float3 skyLight = SampleCloudSkyLight(position, lighting);

    // ========================================================
    // Height Lighting
    // ========================================================

    float h = saturate(
        (position.y - cloudBottom) /
        max(cloudTop - cloudBottom, 1.0f));

    skyLight *= lerp(0.65f, 1.15f, h);

    // ========================================================
    // Silver Lining
    // ========================================================

    float edge = 1.0f - saturate(density * 2.5f);
    float silver = pow(edge, 4.0f);

    sunLight *= 1.0f + silver * 0.35f;

    // ========================================================
    // Final
    // ========================================================

    return sunLight + skyLight;
}

// ============================================================
// Pixel Shader
// ============================================================

float4 PS(PS_INPUT input) : SV_TARGET
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
    // Scene
    // ========================================================

    float3 sceneColor = gSceneColor.SampleLevel(
        gSampler,
        input.texcoord,
        0
    ).rgb;

    // ========================================================
    // Camera Ray
    // ========================================================

    float3 farPosition = ReconstructWorldPosition(
        input.texcoord,
        1.0f);

    float3 rayDirection = SafeNormalize(
        farPosition -
        cameraPosition);

    // ========================================================
    // Cloud Volume Intersection
    // ========================================================

    float cloudEnter;
    float cloudExit;

    if (!RayBox(
        cameraPosition,
        rayDirection,
        CLOUD_MIN,
        CLOUD_MAX,
        cloudEnter,
        cloudExit))
    {
        return float4(sceneColor, 1.0f);
    }

    cloudEnter = max(cloudEnter, 0.0f);

    // ========================================================
    // Scene Depth
    // ========================================================

    float sceneDepth = gSceneDepth.SampleLevel(
        gSampler,
        input.texcoord,
        0
    ).r;

    if (sceneDepth < 0.9999f)
    {
        float3 scenePosition = ReconstructWorldPosition(
            input.texcoord,
            sceneDepth);

        float sceneDistance = dot(
            scenePosition -
            cameraPosition,
            rayDirection);

        cloudExit = min(
            cloudExit,
            sceneDistance);
    }

    float rayLength = cloudExit - cloudEnter;

    if (rayLength <= 0.0f)
    {
        return float4(sceneColor, 1.0f);
    }

    // ========================================================
    // Ray March
    // ========================================================

    int samples = clamp(stepCount, 32, 64);

    float marchStep = rayLength / (float) samples;

    float pixelJitter = Hash31(float3(
        floor(input.position.xy),
        1.37f));

    float3 rayPosition = cameraPosition + rayDirection * (
        cloudEnter +
        marchStep * pixelJitter
    );

    // ========================================================
    // Accumulation
    // ========================================================

    float transmittance = 1.0f;

    float3 accumulatedLight = float3(0.0f, 0.0f, 0.0f);

    float3 viewDirection = -rayDirection;

    // ========================================================
    // Ray March
    // ========================================================

    [loop]
    for (int i = 0; i < 64; ++i)
    {
        if (i >= samples)
            break;

        bool detail = ((i % 2) == 1);

        float density = CloudDensityAt(
            rayPosition,
            detail);

        if (density > 0.001f)
        {
            // =================================================
            // Lighting
            // =================================================

            float3 lightingResult = EvaluateCloudLighting(
                rayPosition,
                density,
                viewDirection,
                lighting);

            // =================================================
            // Density Integration
            // =================================================

            float opticalDepth =
                density *
                absorption *
                marchStep;

            float alpha =
                1.0f -
                exp(-opticalDepth);

            alpha = saturate(alpha);

            // =================================================
            // Accumulation
            // =================================================

            accumulatedLight +=
                lightingResult *
                alpha *
                transmittance;

            transmittance *= 1.0f - alpha;

            // =================================================
            // Early Exit
            // =================================================

            if (transmittance < 0.025f)
            {
                break;
            }
        }

        rayPosition += rayDirection * marchStep;
    }

    // ========================================================
    // Cloud Alpha
    // ========================================================

    float alpha = saturate(1.0f - transmittance);

    if (alpha <= 0.001f)
    {
        return float4(sceneColor, 1.0f);
    }

    // ========================================================
    // Cloud Color
    // ========================================================

    float3 cloudColor =
        accumulatedLight /
        max(alpha, 0.001f);

    // ========================================================
    // HDR Compression
    // ========================================================

    cloudColor =
        cloudColor /
        (1.0f + cloudColor * 0.18f);

    // ========================================================
    // Composite
    // ========================================================

    float3 finalColor = lerp(
        sceneColor,
        cloudColor,
        alpha);

    return float4(
        max(finalColor, 0.0f),
        1.0f);
}