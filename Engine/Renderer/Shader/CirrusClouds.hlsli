// Optically thin high clouds, evaluated at the final pixel resolution.
// World-space sheets provide parallax/occlusion without noisy volume marching.
float CirrusHash(float2 p)
{
    float3 v = frac(float3(p.x, p.y, p.x) * 0.1031f);
    v += dot(v, v.yzx + 33.33f);
    return frac((v.x + v.y) * v.z);
}
float CirrusNoise(float2 p)
{
    float2 cell = floor(p);
    float2 f = frac(p);
    f = f * f * (3 - 2 * f);
    return lerp(lerp(CirrusHash(cell), CirrusHash(cell + float2(1, 0)), f.x),
        lerp(CirrusHash(cell + float2(0, 1)), CirrusHash(cell + 1), f.x), f.y);
}
float CirrusDensity(float2 worldXZ, float cloudTime, float sheet, float footprint)
{
    float2 p = (worldXZ - cloudMotion.yz * cloudTime) * cloudMotion.x * 0.20f;
    p += float2(cloudQuality.z * 0.013f, cloudQuality.z * 0.021f) + sheet * float2(31.7f, 17.3f);
    float2 direction = normalize(float2(0.93f, 0.37f + sheet * 0.12f));
    float2 q = float2(dot(p, direction), dot(p, float2(-direction.y, direction.x)));
    // Long wind-aligned filaments, with broad curvature and soft feathered ends.
    float envelope = CirrusNoise(p * 1.30f + 7.1f) * 0.65f + CirrusNoise(p * 3.1f + 31.2f) * 0.35f;
    float2 bend = float2(CirrusNoise(q * 0.8f + 19.7f), CirrusNoise(q * 0.8f + 67.1f)) - 0.5f;
    q = (q + bend * 1.5f) * float2(0.80f, 2.7f);
    float projected = footprint * cloudMotion.x * 0.20f;
    float broad = lerp(0.5f, CirrusNoise(q), 1 - smoothstep(0.10f, 0.30f, projected));
    float medium = lerp(0.5f, CirrusNoise(q * 2.03f + float2(13.4f, 5.7f)),
        1 - smoothstep(0.04f, 0.12f, projected));
    float fineWeight = 1 - smoothstep(0.015f, 0.06f, projected);
    float fibers = 0.5f;
    if (fineWeight > 0.001f)
        fibers = lerp(0.5f, CirrusNoise(q * float2(1.1f, 5.3f) + 43.1f), fineWeight);
    float streak = broad * 0.62f + medium * 0.28f + fibers * 0.10f;
    float patches = smoothstep(0.48f, 0.72f, envelope + cloudLayer.z * 0.10f);
    return smoothstep(0.34f, 0.70f, streak) * patches;
}
float3 CompositeCirrus(float2 uv, float3 background)
{
    float3 result = background;
    if (cloudMotion.w >= 0.5f && cloudLayer.z > 0)
    {
        float3 ray = CloudViewRay(uv);
        float surface = CloudSceneDistance(uv, ray);
        // From below, cumulus is in front of these high sheets; from above it is behind.
        float foregroundTransmission = ray.y > 0 && cloudLayer.w > 0 ?
            saturate(gClouds.SampleLevel(gSampler, uv, 0).a) : 1;
        float3 scattered = 0;
        float transmission = 1;
        float denominator = (ray.y >= 0 ? 1 : -1) * max(abs(ray.y), 1e-5f);
        [unroll] for (uint i = 0; i < 2; ++i)
        {
            float sheet = ray.y >= 0 ? (float)i : 1.0f - (float)i;
            float altitude = max(14000, cloudLayer.y + 1000) + sheet * 2000;
            float distance = (altitude - cameraPosition.y) / denominator;
            float2 worldXZ = cameraPosition.xz + ray.xz * distance;
            // Grazing rays magnify the sheet footprint much more than t * angular pixel size.
            float footprint = max(length(ddx(worldXZ)), length(ddy(worldXZ)));
            if (distance > 0 && distance < min(surface, 180000))
            {
                float density = CirrusDensity(worldXZ, cloudQuality.w, sheet, footprint);
                float opacity = 1 - exp(-density * lerp(0.26f, 0.16f, sheet));
                opacity *= smoothstep(0.02f, 0.08f, abs(ray.y));
                opacity *= 1 - smoothstep(100000, 180000, distance);
                opacity *= foregroundTransmission;
                // The existing film pass grades the cloud together with the original scene.
                float3 color = float3(0.93f, 0.95f, 0.98f);
                scattered += transmission * opacity * color;
                transmission *= 1 - opacity;
            }
        }
        result = scattered + transmission * background;
    }
    return result;
}
