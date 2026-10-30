// CPUと配置を共有し、定数バッファの成分を用途別の名前で参照する
cbuffer RetroFilmConstants : register(b0)
{
    float time;
    float3 padding;
    float4 colorDesaturationTint;
    float4 colorGainOffset;
    float4 blackLift;
    float4 highlight;
    float4 highlightAddLuminanceR;
    float4 luminanceGBVignette;
    float4 vignetteJitterX;
    float4 jitterYGrain;
    float4 dust;
    float4 dustScratch;
    float4 scratch;
    float4 scratchFlicker;
    float4 overlayColor;
};

Texture2D gSceneColor : register(t0);
SamplerState gSampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float Hash(float2 value)
{
    return frac(sin(dot(value, float2(127.1f, 311.7f))) * 43758.5453f);
}

float FilmGrain(float2 pixel, float time)
{
    const float grainFrameRate = jitterYGrain.y;
    const float grainStrength = jitterYGrain.z;

    float frame = floor(time * grainFrameRate);
    // 大小の粒子を混ぜ、輪郭をぼかさずにフィルムの不均一さを出す
    float fineNoise = Hash(pixel + frame * 17.0f);
    float coarseNoise = Hash(floor(pixel * 0.5f) + frame * 23.0f + 9.7f);
    return ((fineNoise - 0.5f) * 0.75f + (coarseNoise - 0.5f) * 0.25f) * grainStrength;
}

float FilmDust(float2 pixel, float time)
{
    const float dustFrameRate = dust.x;
    const float dustThreshold = dust.y;
    const float minimumDustRadius = dust.z;
    const float maximumDustRadius = dust.w;
    const float dustOpacity = dustScratch.x;

    float cellSize = jitterYGrain.w;
    float2 cell = floor(pixel / cellSize);
    float frame = floor(time * dustFrameRate);
    float chance = Hash(cell + frame * 13.7f + float2(17.1f, 9.2f));

    if (chance < dustThreshold)
        return 0.0f;

    float2 localPixel = frac(pixel / cellSize);
    float2 center = float2(
        Hash(cell + frame * 19.3f + float2(3.1f, 8.7f)),
        Hash(cell + frame * 23.1f + float2(5.2f, 4.6f)));

    float distanceToDust = length((localPixel - center) * cellSize);
    float radius = lerp(minimumDustRadius, maximumDustRadius, Hash(cell + frame * 29.7f));
    return (1.0f - smoothstep(radius * 0.5f, radius, distanceToDust)) * dustOpacity;
}

float FilmScratch(float2 pixel, float time)
{
    const float scratchFrameRate = dustScratch.z;
    const float scratchSpacing = dustScratch.y;
    const float scratchThreshold = dustScratch.w;
    const float minimumScratchWidth = scratch.x;
    const float maximumScratchWidth = scratch.y;
    const float scratchSoftness = scratch.z;
    const float minimumVerticalFade = scratch.w;
    const float scratchOpacity = scratchFlicker.x;

    float frame = floor(time * scratchFrameRate);
    float column = floor(pixel.x / scratchSpacing);
    float chance = Hash(float2(column, frame));

    if (chance < scratchThreshold)
        return 0.0f;

    float position = frac(pixel.x / scratchSpacing);
    float width = lerp(minimumScratchWidth, maximumScratchWidth, Hash(float2(column, frame + 7.3f)));
    float scratchAmount = 1.0f - smoothstep(width, width + scratchSoftness, position);
    float verticalFade = lerp(minimumVerticalFade, 1.0f, Hash(float2(column, frame + 13.1f)));

    return scratchAmount * verticalFade * scratchOpacity;
}

float FilmFlicker(float time)
{
    const float flickerFrameRate = scratchFlicker.y;
    const float flickerStrength = scratchFlicker.z;

    float frame = floor(time * flickerFrameRate);
    float randomValue = Hash(float2(frame, 47.2f));
    return 1.0f + (randomValue - 0.5f) * flickerStrength;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    const float desaturation = colorDesaturationTint.x;
    const float3 colorTint = colorDesaturationTint.yzw;
    const float colorGain = colorGainOffset.x;
    const float3 colorOffset = colorGainOffset.yzw;
    const float horizontalJitterFrequency = vignetteJitterX.w;
    const float horizontalJitterAmount = vignetteJitterX.y;
    const float verticalJitterAmount = vignetteJitterX.z;
    const float verticalJitterFrequency = jitterYGrain.x;
    const float vignetteInnerRadius = luminanceGBVignette.z;
    const float vignetteOuterRadius = luminanceGBVignette.w;
    const float vignetteMinimumBrightness = vignetteJitterX.x;

    float2 pixel = input.position.xy;

    float2 jitter = float2(
        sin(time * horizontalJitterFrequency) * horizontalJitterAmount,
        cos(time * verticalJitterFrequency) * verticalJitterAmount);

    float2 uv = saturate(input.texcoord + jitter);
    float3 color = gSceneColor.SampleLevel(gSampler, uv, 0).rgb;
    float3 luminanceWeights = float3(highlightAddLuminanceR.w, luminanceGBVignette.xy);

    // 昭和の映画をイメージした色調補正
    float luminance = dot(color, luminanceWeights);
    color = lerp(color, luminance.xxx, desaturation);
    color *= colorTint;
    color = color * colorGain + colorOffset;

    // 暗部をわずかに浮かせ、ハイライトを柔らかくする
    color = lerp(blackLift.rgb, color, blackLift.w);
    float highlightAmount = smoothstep(highlight.x, highlight.y, luminance) * highlight.z;
    color = lerp(color, color * highlight.w + highlightAddLuminanceR.rgb, highlightAmount);

    // 色褪せと暖色化による明るさの変化を抑え、機体と背景の判別を保つ
    float gradedLuminance = dot(color, luminanceWeights);
    color += (luminance - gradedLuminance) * 0.65f;

    // 画面端のビネット
    float2 centered = (uv - 0.5f) * 2.0f;
    float vignette = 1.0f - smoothstep(vignetteInnerRadius, vignetteOuterRadius, dot(centered, centered));
    color *= lerp(vignetteMinimumBrightness, 1.0f, vignette);

    // 映写機の微弱な明滅
    color *= FilmFlicker(time);

    // 暗部、白い雲、細い機体の輪郭では粒子を弱める
    float edgeProtection = 1.0f - smoothstep(0.025f, 0.12f, fwidth(luminance));
    float grainExposure = lerp(0.35f, 1.0f, smoothstep(0.04f, 0.25f, luminance));
    grainExposure *= 1.0f - smoothstep(0.72f, 1.0f, luminance) * 0.6f;
    uint width, height;
    gSceneColor.GetDimensions(width, height);
    float2 grainPixel = pixel * (1080.0f / max(float(height), 1.0f));
    color += FilmGrain(grainPixel, time) * grainExposure * lerp(0.25f, 1.0f, edgeProtection);

    // フィルム上の埃と傷
    float dustAmount = FilmDust(pixel, time);
    float scratchAmount = FilmScratch(pixel, time);
    // 照準先となる中央と輪郭では埃や傷を薄くし、視界を遮らない
    float artifactVisibility = lerp(0.25f, 1.0f, smoothstep(0.12f, 0.8f, dot(centered, centered)));
    artifactVisibility *= lerp(0.2f, 1.0f, edgeProtection);
    color += float3(scratchFlicker.w, overlayColor.xy) * saturate(dustAmount + scratchAmount) * artifactVisibility;

    return float4(saturate(color), 1.0f);
}
