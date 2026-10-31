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
    float noise = Hash(pixel + frame * 17.0f);
    return (noise - 0.5f) * grainStrength;
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

    float dustAmount = 0.0f;
    if (chance >= dustThreshold)
    {

        float2 localPixel = frac(pixel / cellSize);
        float2 center = float2(
            Hash(cell + frame * 19.3f + float2(3.1f, 8.7f)), Hash(cell + frame * 23.1f + float2(5.2f, 4.6f)));

        float distanceToDust = length((localPixel - center) * cellSize);
        float radius = lerp(minimumDustRadius, maximumDustRadius, Hash(cell + frame * 29.7f));
        dustAmount = (1.0f - smoothstep(radius * 0.5f, radius, distanceToDust)) * dustOpacity;
    }
    return dustAmount;
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

    float scratchResult = 0.0f;
    if (chance >= scratchThreshold)
    {

        float position = frac(pixel.x / scratchSpacing);
        float width = lerp(minimumScratchWidth, maximumScratchWidth, Hash(float2(column, frame + 7.3f)));
        float scratchAmount = 1.0f - smoothstep(width, width + scratchSoftness, position);
        float verticalFade = lerp(minimumVerticalFade, 1.0f, Hash(float2(column, frame + 13.1f)));

        scratchResult = scratchAmount * verticalFade * scratchOpacity;
    }
    return scratchResult;
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

    float2 jitter = float2(sin(time * horizontalJitterFrequency) * horizontalJitterAmount,
        cos(time * verticalJitterFrequency) * verticalJitterAmount);

    float2 uv = saturate(input.texcoord + jitter);
    float3 color = gSceneColor.SampleLevel(gSampler, uv, 0).rgb;

    // 昭和の映画をイメージした色調補正
    float luminance =
        dot(color, float3(highlightAddLuminanceR.w, luminanceGBVignette.x, luminanceGBVignette.y));
    color = lerp(color, luminance.xxx, desaturation);
    color *= colorTint;
    color = color * colorGain + colorOffset;

    // 暗部をわずかに浮かせ、ハイライトを柔らかくする
    color = lerp(blackLift.rgb, color, blackLift.w);
    float highlightAmount = smoothstep(highlight.x, highlight.y, luminance) * highlight.z;
    color = lerp(color, color * highlight.w + highlightAddLuminanceR.rgb, highlightAmount);

    // 画面端のビネット
    float2 centered = (uv - 0.5f) * 2.0f;
    float vignette = 1.0f - smoothstep(vignetteInnerRadius, vignetteOuterRadius, dot(centered, centered));
    color *= lerp(vignetteMinimumBrightness, 1.0f, vignette);

    // 映写機の微弱な明滅
    color *= FilmFlicker(time);

    // フィルム粒子
    color += FilmGrain(pixel, time);

    // フィルム上の埃と傷
    float dustAmount = FilmDust(pixel, time);
    float scratchAmount = FilmScratch(pixel, time);
    color =
        lerp(color, color + float3(scratchFlicker.w, overlayColor.xy), saturate(dustAmount + scratchAmount));

    return float4(saturate(color), 1.0f);
}
