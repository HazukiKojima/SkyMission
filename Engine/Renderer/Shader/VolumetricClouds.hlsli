#include "CloudCommon.hlsli"

float CloudHash(float3 p)
{
    p = frac(p * 0.1031f);
    p += dot(p, p.yzx + 33.33f);
    return frac((p.x + p.y) * p.z);
}

float CloudNoise(float3 p)
{
    float3 cell = floor(p);
    float3 f = frac(p);
    f = f * f * (3 - 2 * f);

    float n00 = lerp(CloudHash(cell), CloudHash(cell + float3(1, 0, 0)), f.x);
    float n10 = lerp(CloudHash(cell + float3(0, 1, 0)), CloudHash(cell + float3(1, 1, 0)), f.x);
    float n01 = lerp(CloudHash(cell + float3(0, 0, 1)), CloudHash(cell + float3(1, 0, 1)), f.x);
    float n11 = lerp(CloudHash(cell + float3(0, 1, 1)), CloudHash(cell + float3(1, 1, 1)), f.x);
    return lerp(lerp(n00, n10, f.y), lerp(n01, n11, f.y), f.z);
}

// 天候と密度は、ワールド空間で連続した場として扱う。雲の中心、
// 境界の楕円体、セルごとの出現判定、プレイヤー周囲の除外リングは持たない
float4 CloudWeatherCorners(float2 cell, uint band)
{
    float seed = cloudQuality.z * 0.017f + band * 19.13f;
    return float4(CloudHash(float3(cell.x, seed, cell.y)),
        CloudHash(float3(cell.x + 1, seed, cell.y)),
        CloudHash(float3(cell.x, seed, cell.y + 1)),
        CloudHash(float3(cell.x + 1, seed, cell.y + 1)));
}

float2 CloudWeatherPosition(float3 position, float cloudTime)
{
    return (position.xz - cloudMotion.yz * cloudTime) * cloudMotion.x * 0.22f +
        float2(cloudQuality.z * 0.013f, cloudQuality.z * 0.021f);
}

float CloudWeatherMinimum(uint band)
{
    // 積雲だけの被覆率を一時的に下げる。巻雲は元の被覆率を保つ
    const float cumulusCoverageScale = 1.0f / 12.0f;
    return lerp(0.58f, 0.20f, cloudLayer.z * cumulusCoverageScale) + band * 0.06f;
}

float CloudWeather(float3 position, float cloudTime, uint band)
{
    float2 p = CloudWeatherPosition(position, cloudTime);
    float2 f = frac(p);
    f = f * f * (3 - 2 * f);

    float4 corners = CloudWeatherCorners(floor(p), band);
    return lerp(lerp(corners.x, corners.y, f.x), lerp(corners.z, corners.w, f.x), f.y);
}

float CloudDevelopment(float2 p)
{
    float2 cell = floor(p);
    float2 f = frac(p);
    f = f * f * (3 - 2 * f);

    float salt = cloudQuality.z * 0.037f + 57.1f;
    float low = lerp(CloudHash(float3(cell.x, salt, cell.y)),
        CloudHash(float3(cell.x + 1, salt, cell.y)), f.x);
    float high = lerp(CloudHash(float3(cell.x, salt, cell.y + 1)),
        CloudHash(float3(cell.x + 1, salt, cell.y + 1)), f.x);
    float strength = saturate(lerp(low, high, f.y));
    return strength * sqrt(strength);
}

float CloudBandDensity(float3 position, float cloudTime, float footprint, uint band)
{
    float density = 0;
    float layerHeight = cloudLayer.y - cloudLayer.x;
    float bandBottom = cloudLayer.x + band * 0.31f * layerHeight;
    float bandHeight = (0.50f - band * 0.06f) * layerHeight;
    float h = (position.y - bandBottom) / bandHeight;

    if (h > 0 && h < 1)
    {
        float weather = CloudWeather(position, cloudTime, band);
        float amount = smoothstep(CloudWeatherMinimum(band), CloudWeatherMinimum(band) + 0.26f, weather);
        if (amount > 0.001f)
        {
            float3 p = (position - float3(cloudMotion.y, 0, cloudMotion.z) * cloudTime) * cloudMotion.x;
            p += float3(cloudQuality.z * 0.013f, 17.3f, cloudQuality.z * 0.021f);
            p += float3(band * 23.7f, band * 11.3f, band * 41.1f);

            // 各層は、雲底の高さと発達の高さを独立して変化させる
            float development = CloudDevelopment(p.xz * 0.70f);
            float lift = CloudDevelopment(p.xz * 0.29f + float2(13.4f, 7.8f));
            float bottom = 0.03f + (weather - 0.5f) * 0.06f + lift * (band == 0 ? 0.08f : 0.25f);
            float top = bottom + lerp(0.20f, 0.88f - bottom, development);
            float vertical = saturate((h - bottom) / (top - bottom));
            float profile = smoothstep(0, 0.16f, vertical) * (1 - smoothstep(0.72f, 1, vertical));

            if (profile > 0)
            {
                // 本体を縦に伸ばし、背の高い雲柱が、つながった下層の雲から育つようにする
                float3 body = p;
                body.y = (position.y - bandBottom) * cloudMotion.x * 0.55f + 17.3f + band * 11.3f;

                float coarseWeight = 1 - smoothstep(0.40f, 1.0f, footprint * cloudMotion.x);
                float mediumWeight = 1 - smoothstep(0.12f, 0.40f, footprint * cloudMotion.x);
                float coarse = 0.5f, medium = 0.5f;
                [branch] if (coarseWeight > 0)
                    coarse = lerp(0.5f, CloudNoise(body * 1.4f), coarseWeight);
                [branch] if (mediumWeight > 0)
                    medium = lerp(0.5f, CloudNoise(body * 3.3f + float3(7.1f, 13.4f, 2.8f)), mediumWeight);
                float shape = coarse * 0.68f + medium * 0.32f;
                float field = shape - (0.68f - amount * 0.28f) - smoothstep(0.55f, 1, vertical) * 0.12f;
                field += development * smoothstep(0.15f, 0.60f, vertical) * 0.06f;

                // 大きなもこもことした形を保ち、解像しきれない細かいセル状の侵食は加えない
                density = smoothstep(-0.015f, 0.14f, field) * profile * amount * (1 - band * 0.15f);
            }
        }
    }
    return density;
}

uint FirstCloudBand(float h)
{
    return h >= 0.69721f ? 2 : (h >= 0.44001f ? 1 : 0);
}

uint LastCloudBand(float h)
{
    return h > 0.62f ? 2 : (h > 0.31f ? 1 : 0);
}

float CloudDensity(float3 position, float cloudTime, float footprint)
{
    float density = 0;
    float h = (position.y - cloudLayer.x) / (cloudLayer.y - cloudLayer.x);

    // 2層を固定にして、負荷の高い主光線と影の経路で、動的なループや添字のコストを避ける
    if (h > 0 && h < 0.44001f)
        density += CloudBandDensity(position, cloudTime, footprint, 0);
    if (h > 0.31f && h < 0.69721f)
        density += CloudBandDensity(position, cloudTime, footprint, 1);
    return density;
}

float CloudBandBoundaryDistance(float3 position, float3 ray)
{
    float height = cloudLayer.y - cloudLayer.x;
    float h = (position.y - cloudLayer.x) / height;
    float next = 0;

    if (ray.y >= 0)
        next = h < 0.31f ? 0.31f : (h < 0.50f ? 0.50f : (h < 0.62f ? 0.62f : (h < 0.75f ? 0.75f : 1)));
    else
        next = h > 0.75f ? 0.75f : (h > 0.62f ? 0.62f : (h > 0.50f ? 0.50f : (h > 0.31f ? 0.31f : 0)));
    return abs(ray.y) < 1e-6f ? 1e8f : abs((next - h) * height / ray.y);
}

float CloudTransmission(float3 position, float3 lightDirection, float cloudTime,
    float footprint, float distanceAlongRay, float jitter)
{
    float opticalDepth = 0;
    float boundary = lightDirection.y >= 0 ? cloudLayer.y : cloudLayer.x;
    float exit = min(5000, max(0, (boundary - position.y) /
        (abs(lightDirection.y) > 1e-5f ? lightDirection.y : 1e-5f)));
    if (abs(lightDirection.y) <= 1e-5f) exit = 5000;

    // 光源方向の密度を少数点で積分し、遠方では評価数を減らす
    float samples = lerp(3, 2, smoothstep(12000, 60000, distanceAlongRay));
    float previous = 0;
    [loop] for (uint i = 0; i < 3; ++i)
    {
        if ((float)i >= samples || opticalDepth > 6) break;

        float fraction = min((i + 1.0f) / samples, 1);
        float next = exit * fraction * fraction;
        float distance = lerp(previous, next, 0.35f + jitter * 0.3f);
        opticalDepth += CloudDensity(position + lightDirection * distance, cloudTime,
            max(footprint, (next - previous) * 0.5f)) * (next - previous) * cloudLayer.w;
        previous = next;
    }
    return exp(-opticalDepth);
}

float4 IntegrateClouds(float2 uv, float cloudTime, float2 cloudPixel)
{
    float4 result = float4(0, 0, 0, 1);
    if (cloudMotion.w >= 0.5f && cloudLayer.z > 0 && cloudLayer.w > 0)
    {
        float3 ray = CloudViewRay(uv);
        float angularFootprint = max(length(ddx(ray)), length(ddy(ray)));
        float4 intervals = 0;
        CloudShellIntervals(ray, intervals);

        float begin = intervals.x;
        float end = min(intervals.y, min(CloudSceneDistance(uv, ray), 160000));

        // 密度の分布は、設定した殻の高さいっぱいには届かない
        // 確実に空の空間だけを切り捨て、見えるもこもこは全て残す
        float densityTop = cloudLayer.x + (cloudLayer.y - cloudLayer.x) * 0.69721f;
        if (ray.y > 1e-6f)
            end = min(end, (densityTop - cameraPosition.y) / ray.y);
        else if (ray.y < -1e-6f)
            begin = max(begin, (densityTop - cameraPosition.y) / ray.y);
        else if (cameraPosition.y >= densityTop)
            end = begin;
        float3 lightDirection = SafeNormalize(sunDirection);
        CommonLightingParameters lighting;
        lighting.sunDirection = sunDirection;
        lighting.sunIntensity = sunIntensity;
        lighting.sunColor = sunColor;
        lighting.ambientIntensity = ambientIntensity;
        lighting.ambientColor = ambientColor;
        lighting.padding = 0;

        float3 atmosphere = ToneMapReinhard(GetSkyColor(ray, lighting));
        float weatherCellSize = 1 / (cloudMotion.x * 0.22f);
        uint budget = min(512, (uint)cloudQuality.y * 8);
        float qualityScale = 48 / cloudQuality.y;
        float2 cachedWeatherCell = float2(1e20f, 1e20f);
        float3 weatherBounds = 0;
        float lightBegin = -1, lightEnd = -1;
        float lightA = 1, lightB = 1;
        float previousDensityEnd = -1, previousDensity = 0;
        [loop] for (uint i = 0; i < 512; ++i)
        {
            if (i >= budget || begin >= end || result.a < 0.015f) break;

            float3 startPosition = cameraPosition + ray * begin;
            float2 weatherPosition = CloudWeatherPosition(startPosition, cloudTime);

            // 1/8セルのタイルで天候の範囲を安全側に見積もり、密度の計算前に晴天を除外する
            const float weatherTile = 0.125f;
            float2 cell = floor(weatherPosition / weatherTile) * weatherTile;
            if (any(cell != cachedWeatherCell))
            {
                // 滑らかな補間は軸ごとに単調なので、タイルの4隅の値が、
                // 粗いセルの境界付近を含め、タイル内の全ての天候値の範囲を与える
                float2 tileLow = frac(cell);
                float2 tileHigh = tileLow + weatherTile;
                tileLow = tileLow * tileLow * (3 - 2 * tileLow);
                tileHigh = tileHigh * tileHigh * (3 - 2 * tileHigh);
                [unroll] for (uint band = 0; band < 2; ++band)
                {
                    float4 corners = CloudWeatherCorners(floor(cell), band);
                    float2 rowsLow = lerp(corners.xz, corners.yw, tileLow.x);
                    float2 rowsHigh = lerp(corners.xz, corners.yw, tileHigh.x);
                    float4 bounds = float4(lerp(rowsLow.x, rowsLow.y, tileLow.y),
                        lerp(rowsLow.x, rowsLow.y, tileHigh.y),
                        lerp(rowsHigh.x, rowsHigh.y, tileLow.y),
                        lerp(rowsHigh.x, rowsHigh.y, tileHigh.y));
                    weatherBounds[band] = max(max(bounds.x, bounds.y), max(bounds.z, bounds.w));
                }

                cachedWeatherCell = cell;
            }

            float h = saturate((startPosition.y - cloudLayer.x) / (cloudLayer.y - cloudLayer.x));
            bool empty = true;
            uint last = min(1, LastCloudBand(h));
            [loop] for (uint band = FirstCloudBand(h); band <= last; ++band)
            {
                float upperWeather = weatherBounds[band];

                if (upperWeather > CloudWeatherMinimum(band)) empty = false;
            }

            if (empty)
            {
                float2 boundary = cell + float2(ray.x >= 0 ? weatherTile : 0, ray.z >= 0 ? weatherTile : 0);
                float2 crossing = abs(boundary - weatherPosition) * weatherCellSize /
                    max(abs(ray.xz), 1e-8f);
                // 下層が晴れていても、その上の雲が晴れているとは限らない
                float skip = min(min(crossing.x, crossing.y), CloudBandBoundaryDistance(startPosition, ray));
                begin += max(1, skip);
            }
            else
            {
                // 対数間隔にすることで、限られたステップ数でも水平線の先まで届く
                float stepLength = min(end - begin, clamp(begin * 0.025f, 120, 600) * qualityScale);
                float distanceAlongRay = begin + 0.5f * stepLength;
                float3 position = cameraPosition + ray * distanceAlongRay;
                float footprint = distanceAlongRay * angularFootprint;

                // 滑らかな各区間の両端を積分し、前の端点を再利用する
                // ランダムな粒状感を避け、少ない密度評価で輪郭を安定させる
                float densityStart = abs(begin - previousDensityEnd) < 0.5f ? previousDensity :
                    CloudDensity(startPosition, cloudTime, begin * angularFootprint);
                previousDensityEnd = begin + stepLength;
                previousDensity = CloudDensity(cameraPosition + ray * previousDensityEnd,
                    cloudTime, previousDensityEnd * angularFootprint);
                float density = (densityStart + previousDensity) * 0.5f;

                if (density > 0.001f)
                {
                    // 太陽光は輪郭よりもゆっくり変化するので、端点のサンプルを共有する
                    // 最初に密度のあったサンプルに依存せず、固定の距離の節点を使う
                    // 端点の範囲をそろえることで、キャッシュ更新時の影の細部の変化を防ぐ
                    float lightInterval = 1200 * qualityScale;

                    if (distanceAlongRay >= lightEnd)
                    {
                        float nextBegin = floor(distanceAlongRay / lightInterval) * lightInterval;
                        if (abs(nextBegin - lightEnd) < 0.5f)
                            lightA = lightB;
                        else
                            lightA = CloudTransmission(cameraPosition + ray * nextBegin, lightDirection,
                                cloudTime, nextBegin * angularFootprint, nextBegin, 0.5f);
                        lightBegin = nextBegin;
                        lightEnd = lightBegin + lightInterval;
                        lightB = CloudTransmission(cameraPosition + ray * lightEnd, lightDirection,
                            cloudTime, lightEnd * angularFootprint, lightEnd, 0.5f);
                    }

                    float transmission = lerp(lightA, lightB,
                        saturate((distanceAlongRay - lightBegin) / (lightEnd - lightBegin)));
                    float h = saturate((position.y - cloudLayer.x) / (cloudLayer.y - cloudLayer.x));
                    float3 illumination = ambientColor * ambientIntensity * (0.8f + h * 0.5f) +
                        sunColor * sunIntensity * (0.16f + transmission * 0.84f);
                    float3 color = ToneMapReinhard(illumination);
                    float aerial = 1 - exp(-distanceAlongRay / max(8000, cloudQuality.x));
                    color = lerp(color, atmosphere, aerial * 0.85f);

                    float extinction = density * cloudLayer.w * stepLength;
                    extinction *= 1 - smoothstep(80000, 160000, distanceAlongRay);
                    float opacity = 1 - exp(-extinction);
                    result.rgb += result.a * opacity * color;
                    result.a *= 1 - opacity;
                }

                begin += stepLength;
            }
        }
    }
    return result;
}
