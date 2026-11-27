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

#define FOG_CONSTANT_REGISTER b1
#include "OceanFog.hlsli"
#include "CloudComposite.hlsli"
#include "CirrusClouds.hlsli"

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float Hash(float2 value)
{
    return frac(sin(dot(value, float2(127.1f, 311.7f))) * 43758.5453f);
}

float FilmGrain(float2 pixel, float time, float luminance)
{
    const float fineGrainFrameStride = 17.0f;
    const float coarseGrainFrameStride = 23.0f;
    const float coarseGrainFrequencyScale = 0.55f;
    const float coarseGrainBlendWeight = 0.4f;
    const float highlightGrainWeight = 0.45f;
    const float grainHighlightFadeStart = 0.15f;
    const float grainHighlightFadeEnd = 0.85f;
    const float grainFrameRate = jitterYGrain.y;
    const float grainStrength = jitterYGrain.z;

    float frame = floor(time * grainFrameRate);
    // 細粒と粗粒を重ね、解像度が高くてもフィルムの乳剤らしいむらを残す
    float fineNoise = Hash(floor(pixel) + frame * fineGrainFrameStride);
    float2 grainPosition = pixel * coarseGrainFrequencyScale;
    float2 grainCell = floor(grainPosition);
    float2 grainBlend = smoothstep(0.0f, 1.0f, frac(grainPosition));
    float grainFrameOffset = frame * coarseGrainFrameStride;
    float coarseNoise = lerp(
        lerp(Hash(grainCell + grainFrameOffset), Hash(grainCell + float2(1, 0) + grainFrameOffset), grainBlend.x),
        lerp(Hash(grainCell + float2(0, 1) + grainFrameOffset),
            Hash(grainCell + float2(1, 1) + grainFrameOffset), grainBlend.x), grainBlend.y);
    float noise = lerp(fineNoise, coarseNoise, coarseGrainBlendWeight);

    // フィルムの粒子は、明るいハイライトよりも中間調と影で目立つ
    float exposureWeight = lerp(1.0f, highlightGrainWeight,
        smoothstep(grainHighlightFadeStart, grainHighlightFadeEnd, luminance));
    return (noise - 0.5f) * grainStrength * exposureWeight;
}

float FilmDust(float2 pixel, float time)
{
    const float dustChanceFrameStride = 13.7f;
    const float2 dustChanceOffset = float2(17.1f, 9.2f);
    const float dustCenterXFrameStride = 19.3f;
    const float dustCenterYFrameStride = 23.1f;
    const float2 dustCenterXOffset = float2(3.1f, 8.7f);
    const float2 dustCenterYOffset = float2(5.2f, 4.6f);
    const float dustRadiusFrameStride = 29.7f;
    const float dustFeatherStartFraction = 0.5f;
    const float dustFrameRate = dust.x;
    const float dustThreshold = dust.y;
    const float minimumDustRadius = dust.z;
    const float maximumDustRadius = dust.w;
    const float dustOpacity = dustScratch.x;

    float cellSize = jitterYGrain.w;
    float2 cell = floor(pixel / cellSize);
    float frame = floor(time * dustFrameRate);
    float chance = Hash(cell + frame * dustChanceFrameStride + dustChanceOffset);

    // 選ばれたセル内にランダムな位置と大きさの埃を置く
    float dustAmount = 0.0f;
    if (chance >= dustThreshold)
    {

        float2 localPixel = frac(pixel / cellSize);
        float2 center = float2(
            Hash(cell + frame * dustCenterXFrameStride + dustCenterXOffset),
            Hash(cell + frame * dustCenterYFrameStride + dustCenterYOffset));

        float distanceToDust = length((localPixel - center) * cellSize);
        float radius = lerp(minimumDustRadius, maximumDustRadius, Hash(cell + frame * dustRadiusFrameStride));
        dustAmount = (1.0f - smoothstep(radius * dustFeatherStartFraction, radius, distanceToDust)) * dustOpacity;
    }
    return dustAmount;
}

float FilmScratch(float2 pixel, float time)
{
    const float scratchCenterMin = 0.18f;
    const float scratchCenterMax = 0.82f;
    const float scratchWidthSeedOffset = 7.3f;
    const float scratchCenterSeedOffset = 31.4f;
    const float scratchWearSeedOffset = 13.1f;
    const float minimumScratchFeatherPixels = 0.6f;
    const float scratchWearCellPixels = 180.0f;
    const float scratchWearFadeStart = 0.2f;
    const float scratchWearFadeEnd = 0.8f;
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
        float width = lerp(minimumScratchWidth, maximumScratchWidth,
            Hash(float2(column, frame + scratchWidthSeedOffset)));
        float center = lerp(scratchCenterMin, scratchCenterMax, Hash(float2(column, frame + scratchCenterSeedOffset)));

        // サブピクセルの傷には、ピクセル大のぼかしが必要。ないと線がどのピクセルにも掛からないことがある
        float softness = max(scratchSoftness, minimumScratchFeatherPixels / max(scratchSpacing, 1.0f));
        float scratchAmount = 1.0f - smoothstep(width, width + softness, abs(position - center));
        float verticalCell = pixel.y / scratchWearCellPixels;
        float verticalBlend = frac(verticalCell);
        verticalBlend = verticalBlend * verticalBlend * (3 - 2 * verticalBlend);

        float wear = lerp(Hash(float2(column + floor(verticalCell), frame + scratchWearSeedOffset)),
            Hash(float2(column + floor(verticalCell) + 1, frame + scratchWearSeedOffset)), verticalBlend);
        float verticalFade = lerp(minimumVerticalFade, 1.0f, smoothstep(scratchWearFadeStart, scratchWearFadeEnd, wear));

        scratchResult = scratchAmount * verticalFade * scratchOpacity;
    }
    return scratchResult;
}

float FilmFlicker(float time)
{
    const float flickerSeed = 47.2f;
    const float flickerFrameRate = scratchFlicker.y;
    const float flickerStrength = scratchFlicker.z;

    float frame = floor(time * flickerFrameRate);
    float randomValue = Hash(float2(frame, flickerSeed));
    return 1.0f + (randomValue - 0.5f) * flickerStrength;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    const float2 gateWeaveSeeds = float2(11.3f, 29.7f);
    const float periodicJitterWeight = 0.7f;
    const float randomJitterWeight = 0.3f;
    const float highlightShoulderSoftness = 0.35f;
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

    // フレーム単位の不規則な揺れを混ぜ、周期的なカメラ移動に見えるのを避ける
    float filmFrame = floor(time * jitterYGrain.y);
    float2 gateWeave = float2(Hash(float2(filmFrame, gateWeaveSeeds.x)),
        Hash(float2(filmFrame, gateWeaveSeeds.y))) * 2 - 1;
    float2 jitter = float2(
        (sin(time * horizontalJitterFrequency) * periodicJitterWeight + gateWeave.x * randomJitterWeight) *
            horizontalJitterAmount,
        (cos(time * verticalJitterFrequency) * periodicJitterWeight + gateWeave.y * randomJitterWeight) *
            verticalJitterAmount);

    float2 uv = saturate(input.texcoord + jitter);
    uint sceneWidth, sceneHeight;
    gSceneColor.GetDimensions(sceneWidth, sceneHeight);
    float3 color = ApplyOceanFog(uv, 1.0f / max((float)sceneHeight, 1.0f)).rgb;
    color = CompositeClouds(uv, color);
    color = CompositeCirrus(uv, color);

    // 昭和の映画をイメージした色調補正
    float luminance =
        dot(color, float3(highlightAddLuminanceR.w, luminanceGBVignette.x, luminanceGBVignette.y));
    color = lerp(color, luminance.xxx, desaturation);
    color *= colorTint;
    color = color * colorGain + colorOffset;

    // 乳剤の肩特性を近似し、明部の階調を残したまま白を柔らかくする
    float3 highlightExcess = max(color - highlight.x, 0.0f);
    color -= highlightExcess * highlightExcess / (highlightExcess + highlightShoulderSoftness) * highlight.z;

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

    // 粒子を雲の上にも重ね、画面全体の質感をそろえる
    color += FilmGrain(pixel, time, saturate(luminance));

    // フィルム上の埃と傷
    float dustAmount = FilmDust(pixel, time);
    float scratchAmount = FilmScratch(pixel, time);
    color =
        lerp(color, color + float3(scratchFlicker.w, overlayColor.xy), saturate(dustAmount + scratchAmount));

    return float4(saturate(color), 1.0f);
}
