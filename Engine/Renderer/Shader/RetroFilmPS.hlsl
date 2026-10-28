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
    float frame = floor(time * jitterYGrain.y);
    float noise = Hash(pixel + frame * 17.0f);
    return (noise - 0.5f) * jitterYGrain.z;
}

float FilmDust(float2 pixel, float time)
{
    float cellSize = jitterYGrain.w;
    float2 cell = floor(pixel / cellSize);
    float frame = floor(time * dust.x);
    float chance = Hash(cell + frame * 13.7f + float2(17.1f, 9.2f));

    if (chance < dust.y)
        return 0.0f;

    float2 localPixel = frac(pixel / cellSize);
    float2 center = float2(
        Hash(cell + frame * 19.3f + float2(3.1f, 8.7f)),
        Hash(cell + frame * 23.1f + float2(5.2f, 4.6f)));

    float distanceToDust = length((localPixel - center) * cellSize);
    float radius = lerp(dust.z, dust.w, Hash(cell + frame * 29.7f));
    return (1.0f - smoothstep(radius * 0.5f, radius, distanceToDust)) * dustScratch.x;
}

float FilmScratch(float2 pixel, float time)
{
    float frame = floor(time * dustScratch.z);
    float column = floor(pixel.x / dustScratch.y);
    float chance = Hash(float2(column, frame));

    if (chance < dustScratch.w)
        return 0.0f;

    float position = frac(pixel.x / dustScratch.y);
    float width = lerp(scratch.x, scratch.y, Hash(float2(column, frame + 7.3f)));
    float scratchAmount = 1.0f - smoothstep(width, width + scratch.z, position);
    float verticalFade = lerp(scratch.w, 1.0f, Hash(float2(column, frame + 13.1f)));

    return scratchAmount * verticalFade * scratchFlicker.x;
}

float FilmFlicker(float time)
{
    float frame = floor(time * scratchFlicker.y);
    float randomValue = Hash(float2(frame, 47.2f));
    return 1.0f + (randomValue - 0.5f) * scratchFlicker.z;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    float2 pixel = input.position.xy;

    float2 jitter = float2(
        sin(time * vignetteJitterX.w) * vignetteJitterX.y,
        cos(time * jitterYGrain.x) * vignetteJitterX.z);

    float2 uv = saturate(input.texcoord + jitter);
    float3 color = gSceneColor.SampleLevel(gSampler, uv, 0).rgb;

    // 昭和の映画をイメージした色調補正
    float luminance = dot(color, float3(
        highlightAddLuminanceR.w,
        luminanceGBVignette.x,
        luminanceGBVignette.y));
    color = lerp(color, luminance.xxx, colorDesaturationTint.x);
    color *= colorDesaturationTint.yzw;
    color = color * colorGainOffset.x + colorGainOffset.yzw;

    // 暗部をわずかに浮かせ、ハイライトを柔らかくする
    color = lerp(blackLift.rgb, color, blackLift.w);
    float highlightAmount = smoothstep(highlight.x, highlight.y, luminance) * highlight.z;
    color = lerp(color, color * highlight.w + highlightAddLuminanceR.rgb, highlightAmount);

    // 画面端のビネット
    float2 centered = (uv - 0.5f) * 2.0f;
    float vignette = 1.0f - smoothstep(luminanceGBVignette.z, luminanceGBVignette.w, dot(centered, centered));
    color *= lerp(vignetteJitterX.x, 1.0f, vignette);

    // 映写機の微弱な明滅
    color *= FilmFlicker(time);

    // フィルム粒子
    color += FilmGrain(pixel, time);

    // フィルム上の埃と傷
    float dust = FilmDust(pixel, time);
    float scratch = FilmScratch(pixel, time);
    color = lerp(color, color + float3(scratchFlicker.w, overlayColor.xy), saturate(dust + scratch));

    return float4(saturate(color), 1.0f);
}