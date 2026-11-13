float3 CloudViewRay(float2 uv)
{
    float2 ndc = uv * float2(2, -2) + float2(-1, 1);
    float4 farH = mul(float4(ndc, 1, 1), inverseViewProjection);
    return SafeNormalize(farH.xyz);
}
float CloudSceneDistance(float2 uv, float3 ray)
{
    float distanceToSurface = 1e8f;
    uint width, height;
    gSceneDepth.GetDimensions(width, height);
    int2 pixel = clamp(int2(uv * float2(width, height)), int2(0, 0), int2(width, height) - 1);
    float depth = gSceneDepth.Load(int3(pixel, 0)).r;
    if (depth < 1)
    {
        float2 ndc = uv * float2(2, -2) + float2(-1, 1);
        float4 hitH = mul(float4(ndc, depth, 1), inverseViewProjection);
        distanceToSurface = max(0, dot(hitH.xyz / max(hitH.w, 1e-8f), ray));
    }
    return distanceToSurface;
}
// 海面と同じワールド座標の雲層。遠景も連続した密度場として扱う
void CloudShellIntervals(float3 ray, out float4 intervals)
{
    intervals = 0;
    if (abs(ray.y) > 1e-6f)
    {
        float bottom = (cloudLayer.x - cameraPosition.y) / ray.y;
        float top = (cloudLayer.y - cameraPosition.y) / ray.y;
        float begin = max(0, min(bottom, top));
        float end = max(bottom, top);
        if (end > begin)
            intervals.xy = float2(begin, end);
    }
    else if (cameraPosition.y > cloudLayer.x && cameraPosition.y < cloudLayer.y)
        intervals.xy = float2(0, 1e8f);
}
float FirstCloudDistance(float3 ray)
{
    float4 intervals = 0;
    CloudShellIntervals(ray, intervals);
    float begin = 1e8f;
    if (intervals.y > intervals.x)
        begin = intervals.x;
    if (intervals.w > intervals.z)
        begin = min(begin, intervals.z);
    return begin;
}
