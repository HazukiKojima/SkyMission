Texture2D atlas : register(t0);
Texture2D<float> sceneDepth : register(t1);
SamplerState atlasSampler : register(s0);

static const int kTurbulenceOctaves = 4;
static const float kTurbulenceAmplitudeDecay = 0.5f;
static const float kProceduralUvThreshold = -0.5f;
static const float kProceduralSeedStride = 4.0f;
static const float kFireRedChannelThreshold = 0.8f;
float Hash(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

float Noise(float2 p)
{
    // 格子点の乱数を滑らかに補間する
    float2 cell = floor(p), f = frac(p);
    f = f * f * (3 - 2 * f);
    return lerp(lerp(Hash(cell), Hash(cell + float2(1, 0)), f.x),
        lerp(Hash(cell + float2(0, 1)), Hash(cell + 1), f.x), f.y);
}

float Turbulence(float2 p)
{
    // 異なる大きさのノイズを重ねて細部を加える
    const float initialWeight = 0.55;
    const float2x2 octaveTransform = float2x2(1.6, -1.2, 1.2, 1.6);
    const float octaveTranslation = 7.3;
    float value = 0, weight = initialWeight;
    [unroll] for (int i = 0; i < kTurbulenceOctaves; ++i)
    {
        value += Noise(p) * weight;
        p = mul(octaveTransform, p) + octaveTranslation;
        weight *= kTurbulenceAmplitudeDecay;
    }
    return value;
}

float4 PS(float4 position : SV_POSITION, float2 uv : TEXCOORD0, float4 color : COLOR0,
    noperspective float effectDepth : TEXCOORD1) : SV_TARGET
{
    // ワールドエフェクトのうち、手前のシーン形状に隠れる部分を除く
    [branch]
    if (effectDepth >= 0.0f)
        clip(sceneDepth.Load(int3(int2(position.xy), 0)) - effectDepth);

    // 負のU座標は文字ではなく煙や炎として描く
    if (uv.x < kProceduralUvThreshold)
    {
        const float2 seedOffsetScale = float2(3.17, 7.31);
        const float noiseFrequency = 3.2;
        const float shapeNoiseStrength = 0.65;
        const float edgeFadeEnd = 0.38;
        const float2 densityNoiseRange = float2(0.15, 0.72);
        const float baseLight = 0.65;
        const float verticalLightGradient = 0.25;
        const float lightNoiseStrength = 0.7;
        const float minimumTintBrightness = 0.5;
        const float tintLightStrength = 0.8;
        const float fireHeatThreshold = 0.25;
        const float fireHeatGain = 1.8;
        const float3 coolFireColor = float3(0.4, 0.045, 0.008);
        const float3 hotFireColor = float3(1, 0.9, 0.55);
        const float fireHighlightExponent = 4;
        const float fireHighlightStrength = 0.65;
        float seed = floor(uv.y / kProceduralSeedStride);
        float2 p = float2(uv.x + 2, uv.y - seed * kProceduralSeedStride - 1);
        float2 offset = seed * seedOffsetScale;
        float n = Turbulence(p * noiseFrequency + offset);
        float shape = 1 - length(p) + (n - 0.5) * shapeNoiseStrength;
        float density = smoothstep(0, edgeFadeEnd, shape) * smoothstep(densityNoiseRange.x, densityNoiseRange.y, n);
        float light = saturate(baseLight - p.y * verticalLightGradient + (n - 0.5) * lightNoiseStrength);
        float3 tint = color.rgb * (minimumTintBrightness + light * tintLightStrength);

        if (color.r > kFireRedChannelThreshold)
        {
            float heat = saturate((n - fireHeatThreshold) * fireHeatGain);
            tint = lerp(coolFireColor, color.rgb, heat);
            tint = lerp(tint, hotFireColor, pow(heat, fireHighlightExponent) * fireHighlightStrength);
        }
        return float4(tint, color.a * density);
    }
    return float4(color.rgb, color.a * atlas.Sample(atlasSampler, uv).a);
}
