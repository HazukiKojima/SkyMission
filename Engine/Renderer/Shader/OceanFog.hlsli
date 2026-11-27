#ifndef FOG_CONSTANT_REGISTER
#define FOG_CONSTANT_REGISTER b0
#endif
#include "CommonLighting.hlsli"

static const float kSkySampleOpacityThreshold = 0.99f;
static const float kOceanMaskThreshold = 0.5f;
static const float kMinimumRayAxisDirection = 1e-5f;
static const float kMinimumVisibleOceanFog = 0.001f;
static const float kMinimumWorldClipW = 1e-8f;
static const float kMinimumFogLayerThickness = 1.0f;
static const float kMinimumHorizontalRaySpeed = 1e-6f;
static const float kMinimumFogEdgeFadeWidth = 1e-6f;
static const float kMinimumFogDistanceScale = 1.0f;
static const float kFogHorizonExponent = 4.0f;
static const float kSceneFarDepth = 1.0f;
static const int kFogQuadraturePointCount = 4;
static const int kSkySearchSampleCount = 8;

cbuffer OceanFogConstants : register(FOG_CONSTANT_REGISTER)
{
    float4x4 inverseViewProjection;
    float3 cameraPosition;
    float padding0;
    float3 sunDirection;
    float sunIntensity;
    float3 sunColor;
    float ambientIntensity;
    float3 ambientColor;
    float oceanFogBottom;
    float oceanFogTop;
    float oceanFogDensity;
    float oceanFogStartDistance;
    float oceanFogEndDistance;
    float oceanEdgeFadeStart;
    float oceanEdgeFadeWidth;
    float2 padding1;
    float4 cloudLayer;
    float4 cloudMotion;
    float4 cloudQuality;
};

Texture2D gSceneColor : register(t0);
Texture2D gSceneDepth : register(t1);
SamplerState gSampler : register(s0);

bool RayBox(float3 origin, float3 direction, float3 boxMin, float3 boxMax, out float tMin, out float tMax)
{
    // ゼロ除算を避けながら各軸の交差区間を求める
    float3 invDirection = 1.0f / max(abs(direction), kMinimumRayAxisDirection);
    invDirection *= float3(direction.x >= 0 ? 1 : -1, direction.y >= 0 ? 1 : -1, direction.z >= 0 ? 1 : -1);

    float3 t0 = (boxMin - origin) * invDirection;
    float3 t1 = (boxMax - origin) * invDirection;
    float3 tSmall = min(t0, t1);
    float3 tLarge = max(t0, t1);

    // 三軸で共通する区間を光線と箱の交差とする
    tMin = max(max(tSmall.x, tSmall.y), tSmall.z);
    tMax = min(min(tLarge.x, tLarge.y), tLarge.z);

    return tMax > max(tMin, 0.0f);
}

float3 ReconstructWorldPosition(float2 uv, float depth)
{
    float2 ndc = uv * 2.0f - 1.0f;
    ndc.y = -ndc.y;

    float4 clip = float4(ndc, depth, 1.0f);
    float4 world = mul(clip, inverseViewProjection);

    // カメラ相対の位置をワールド座標へ戻す
    return cameraPosition + world.xyz / max(world.w, kMinimumWorldClipW);
}

float OceanFogDensityAt(float3 worldPosition)
{
    float height = saturate(
        (worldPosition.y - oceanFogBottom) / max(oceanFogTop - oceanFogBottom, kMinimumFogLayerThickness));
    float heightDensity = 1.0f - smoothstep(0.0f, 1.0f, height);

    // 海の外周へ近づくほど霧を濃くする
    float2 oceanLocalPosition = worldPosition.xz - cameraPosition.xz;
    float oceanDistance = max(abs(oceanLocalPosition.x), abs(oceanLocalPosition.y));
    float edgeDensity = smoothstep(oceanFogStartDistance, oceanFogEndDistance, oceanDistance);

    return heightDensity * edgeDensity;
}

float OceanHorizonFog(float3 rayOrigin, float3 rayDirection, float maxDistance)
{
    float3 fogMin =
        float3(rayOrigin.x - oceanFogEndDistance, oceanFogBottom, rayOrigin.z - oceanFogEndDistance);
    float3 fogMax = float3(rayOrigin.x + oceanFogEndDistance, oceanFogTop, rayOrigin.z + oceanFogEndDistance);

    float fogEnter = 0.0f;
    float fogExit = 0.0f;
    bool intersectsFog = RayBox(rayOrigin, rayDirection, fogMin, fogMax, fogEnter, fogExit);

    fogEnter = max(fogEnter, oceanFogStartDistance);
    fogExit = min(fogExit, min(maxDistance, oceanFogEndDistance));

    // 霧を通る区間だけ積分し、区間がなければ初期値のゼロを返す
    float fogAmount = 0.0f;
    if (intersectsFog && fogExit > fogEnter)
    {

        // 密度は3次スムーズステップの積。4点のガウス＝ルジャンドル求積で、
        // クランプ前の区間における6次多項式を積分する
        float horizontalSpeed = max(abs(rayDirection.x), abs(rayDirection.z));
        fogEnter = max(fogEnter, oceanFogStartDistance / max(horizontalSpeed, kMinimumHorizontalRaySpeed));

        float midpoint = (fogEnter + fogExit) * 0.5f;
        float halfLength = max((fogExit - fogEnter) * 0.5f, 0.0f);
        static const float nodes[kFogQuadraturePointCount] = {
            -0.8611363116f, -0.3399810436f, 0.3399810436f, 0.8611363116f};
        static const float weights[kFogQuadraturePointCount] = {
            0.3478548451f, 0.6521451549f, 0.6521451549f, 0.3478548451f};

        float opticalDepth = 0.0f;
        [unroll] for (int i = 0; i < kFogQuadraturePointCount; ++i)
        {
            float t = midpoint + halfLength * nodes[i];
            opticalDepth += OceanFogDensityAt(rayOrigin + rayDirection * t) * weights[i];
        }

        opticalDepth *= oceanFogDensity * halfLength;

        float horizonMask = pow(1.0f - saturate(abs(rayDirection.y)), kFogHorizonExponent);
        fogAmount = saturate((1.0f - exp(-opticalDepth)) * horizonMask);
    }
    return fogAmount;
}

float3 SampleSceneSkyAbove(float2 uv, float pixelHeight, float3 fallback)
{
    float3 skyColor = fallback;
    bool foundSky = false;

    // 最初に見つけた空の色を保持し、見つからなければ代替色を使う
    [unroll] for (int i = 0; i < kSkySearchSampleCount; ++i)
    {
        if (!foundSky)
        {
            float sampleOffset = pixelHeight * exp2((float)i);
            float2 sampleUv = float2(uv.x, max(uv.y - sampleOffset, pixelHeight * 0.5f));
            float sampleDepth = gSceneDepth.SampleLevel(gSampler, sampleUv, 0).r;

            if (sampleDepth >= kSceneFarDepth)
            {
                float4 sampleColor = gSceneColor.SampleLevel(gSampler, sampleUv, 0);
                if (sampleColor.a > kSkySampleOpacityThreshold)
                {
                    skyColor = sampleColor.rgb;
                    foundSky = true;
                }
            }
        }
    }

    return skyColor;
}

float4 ApplyOceanFog(float2 uv, float pixelHeight)
{
    CommonLightingParameters lighting;
    lighting.sunDirection = sunDirection;
    lighting.sunIntensity = sunIntensity;
    lighting.sunColor = sunColor;
    lighting.ambientIntensity = ambientIntensity;
    lighting.ambientColor = ambientColor;
    lighting.padding = 0.0f;

    float4 sceneSample = gSceneColor.SampleLevel(gSampler, uv, 0);
    float3 sceneColor = sceneSample.rgb;
    bool isOceanPixel = sceneSample.a < kOceanMaskThreshold;
    float sceneDepth = gSceneDepth.SampleLevel(gSampler, uv, 0).r;

    // 逆行列から視線を復元し、霧を積分する方向を求める
    float2 rayNdc = uv * float2(2, -2) + float2(-1, 1);
    float4 farRelative = mul(float4(rayNdc, 1, 1), inverseViewProjection);
    float3 rayDirection = SafeNormalize(farRelative.xyz);

    float oceanFogDistanceLimit = oceanFogEndDistance;
    float sceneDistance = oceanFogEndDistance;
    float oceanEdgeFade = 0.0f;

    // 海面は視線との交点、その他の物体は深度から距離を求める
    if (isOceanPixel)
    {
        if (abs(rayDirection.y) > kMinimumRayAxisDirection)
            sceneDistance = max(-cameraPosition.y / rayDirection.y, 0.0f);

        float3 oceanPosition = cameraPosition + rayDirection * sceneDistance;
        float2 oceanLocalPosition = oceanPosition.xz - cameraPosition.xz;
        float oceanEdgeDistance =
            max(abs(oceanLocalPosition.x), abs(oceanLocalPosition.y)) /
            max(oceanFogEndDistance, kMinimumFogDistanceScale);
        oceanEdgeFade = smoothstep(
            oceanEdgeFadeStart, oceanEdgeFadeStart + max(oceanEdgeFadeWidth, kMinimumFogEdgeFadeWidth), oceanEdgeDistance);
    }
    else if (sceneDepth < kSceneFarDepth)
    {
        float3 scenePosition = ReconstructWorldPosition(uv, sceneDepth);
        sceneDistance = dot(scenePosition - cameraPosition, rayDirection);
    }

    oceanFogDistanceLimit = min(oceanFogDistanceLimit, sceneDistance);

    float oceanFogFactor = 0.0f;
    if (isOceanPixel || sceneDepth < kSceneFarDepth)
    {
        oceanFogFactor = OceanHorizonFog(cameraPosition, rayDirection, oceanFogDistanceLimit);
        oceanFogFactor = max(oceanFogFactor, oceanEdgeFade);
    }

    // 海面では画面内の空の色へなじませ、見つからなければ計算した空色を使う
    float3 horizonFallback = ToneMapReinhard(GetSkyColor(rayDirection, lighting));
    float3 horizonColor = horizonFallback;
    if (isOceanPixel && oceanFogFactor > kMinimumVisibleOceanFog)
        horizonColor = SampleSceneSkyAbove(uv, pixelHeight, horizonFallback);

    sceneColor = lerp(sceneColor, horizonColor, oceanFogFactor);
    return float4(sceneColor, 1.0f);
}
