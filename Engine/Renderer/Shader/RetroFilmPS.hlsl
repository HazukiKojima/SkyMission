cbuffer RetroFilmConstants : register(b0)
{
    float time;
    float3 padding;
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
    float frame = floor(time * 24.0f);
    float noise = Hash(pixel + frame * 17.0f);
    return (noise - 0.5f) * 0.055f;
}

float FilmDust(float2 pixel, float time)
{
    const float cellSize = 80.0f;
    float2 cell = floor(pixel / cellSize);
    float frame = floor(time * 3.0f);
    float chance = Hash(cell + frame * 13.7f + float2(17.1f, 9.2f));

    if (chance < 0.88f)
        return 0.0f;

    float2 localPixel = frac(pixel / cellSize);
    float2 center = float2(
        Hash(cell + frame * 19.3f + float2(3.1f, 8.7f)),
        Hash(cell + frame * 23.1f + float2(5.2f, 4.6f)));

    float distanceToDust = length((localPixel - center) * cellSize);
    float radius = lerp(1.0f, 2.5f, Hash(cell + frame * 29.7f));
    return (1.0f - smoothstep(radius * 0.5f, radius, distanceToDust)) * 0.22f;
}

float FilmScratch(float2 pixel, float time)
{
    float frame = floor(time * 1.5f);
    float column = floor(pixel.x / 17.0f);
    float chance = Hash(float2(column, frame));

    if (chance < 0.965f)
        return 0.0f;

    float position = frac(pixel.x / 17.0f);
    float width = lerp(0.015f, 0.04f, Hash(float2(column, frame + 7.3f)));
    float scratch = 1.0f - smoothstep(width, width + 0.02f, position);
    float verticalFade = lerp(0.35f, 1.0f, Hash(float2(column, frame + 13.1f)));

    return scratch * verticalFade * 0.12f;
}

float FilmFlicker(float time)
{
    float frame = floor(time * 24.0f);
    float randomValue = Hash(float2(frame, 47.2f));
    return 1.0f + (randomValue - 0.5f) * 0.018f;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    float2 pixel = input.position.xy;

    float2 jitter = float2(
        sin(time * 1.7f) * 0.00008f,
        cos(time * 1.3f) * 0.00006f);

    float2 uv = saturate(input.texcoord + jitter);
    float3 color = gSceneColor.SampleLevel(gSampler, uv, 0).rgb;

    // 昭和の映画をイメージした色調補正
    float luminance = dot(color, float3(0.299f, 0.587f, 0.114f));
    color = lerp(color, luminance.xxx, 0.12f);
    color *= float3(1.035f, 1.015f, 0.94f);
    color = color * 1.025f + float3(0.012f, 0.009f, 0.002f);

    // 暗部をわずかに浮かせ、ハイライトを柔らかくする
    color = lerp(float3(0.018f, 0.014f, 0.008f), color, 0.97f);
    float highlight = smoothstep(0.55f, 1.0f, luminance) * 0.08f;
    color = lerp(color, color * 0.92f + float3(0.08f, 0.075f, 0.06f), highlight);

    // 画面端のビネット
    float2 centered = (uv - 0.5f) * 2.0f;
    float vignette = 1.0f - smoothstep(0.25f, 2.0f, dot(centered, centered));
    color *= lerp(0.55f, 1.0f, vignette);

    // 映写機の微弱な明滅
    color *= FilmFlicker(time);

    // フィルム粒子
    color += FilmGrain(pixel, time);

    // フィルム上の埃と傷
    float dust = FilmDust(pixel, time);
    float scratch = FilmScratch(pixel, time);
    color = lerp(color, color + float3(0.24f, 0.22f, 0.18f), saturate(dust + scratch));

    return float4(saturate(color), 1.0f);
}