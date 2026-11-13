#include "CloudCommon.hlsli"
Texture2D gClouds : register(t2);
float3 CompositeClouds(float2 uv, float3 background)
{
    float3 result = background;
    if (cloudMotion.w >= 0.5f && cloudLayer.z > 0 && cloudLayer.w > 0)
    {
        float3 ray = CloudViewRay(uv);
        // フル解像度の深度で再確認し、細い機体の輪郭への雲のにじみを防ぐ
        float surfaceDistance = CloudSceneDistance(uv, ray);
        if (surfaceDistance > FirstCloudDistance(ray))
        {
            uint width, height;
            gClouds.GetDimensions(width, height);
            float2 dimensions = float2(width, height);
            float2 grid = uv * dimensions - 0.5f;
            float2 origin = floor(grid);
            float2 blend = frac(grid);
            float4 clouds = 0;
            float totalWeight = 0;
            // 深度の近いサンプルだけを拡大し、雲の中の機体の輪郭も保つ
            [unroll] for (uint y = 0; y < 2; ++y)[unroll] for (uint x = 0; x < 2; ++x)
            {
                int2 tapPixel = clamp(int2(origin) + int2(x, y), int2(0, 0), int2(width, height) - 1);
                float2 tapUv = (float2(tapPixel) + 0.5f) / dimensions;
                float tapDistance = CloudSceneDistance(tapUv, CloudViewRay(tapUv));
                float weight = (x == 0 ? 1 - blend.x : blend.x) * (y == 0 ? 1 - blend.y : blend.y);
                weight *= exp2(-abs(tapDistance - surfaceDistance) / max(50, surfaceDistance * 0.1f));
                clouds += gClouds.Load(int3(tapPixel, 0)) * weight;
                totalWeight += weight;
            }
            if (totalWeight > 0.001f)
            {
                clouds /= totalWeight;
                result = clouds.rgb + background * saturate(clouds.a);
            }
        }
    }
    return result;
}
