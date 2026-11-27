#include "../Environment/OceanWaveConstants.h"

cbuffer MatrixBuffer : register(b0)
{
    float4x4 mvp;

    float time;
    float3 oceanOffset;

    float3 cameraPos;
    float pad2;

    float4 oceanLightingPadding[3];

    float oceanSize;
    float oceanWaveLength;
    float oceanWaveSteepness;
    float oceanNormalUvScale;
    float oceanWavePatternUvScale;
    float oceanFogStartDistance;
    float oceanFogEndDistance;
    float oceanEdgeFadeStart;
    float oceanEdgeFadeWidth;
    float oceanWaveHeightOffset;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float3 wavePos : TEXCOORD2;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

// ゲルストナー波の変位

float3 CalculateGerstnerWave(float2 dir, float steepness, float wavelength, float3 p, inout float3 tangent,
    inout float3 binormal, float phaseOffset)
{
    float k = 2.0f * 3.14159265359f / wavelength;
    float c = sqrt(kOceanWaveGravity / k);
    float2 d = normalize(dir);

    float f = k * (dot(d, p.xz) - c * time * kOceanWaveTimeScale) + phaseOffset;
    float a = steepness / k;

    float sinf = sin(f);
    float cosf = cos(f);

    // 波の傾きから接線を更新して法線計算に使う
    tangent +=
        float3(-d.x * d.x * (steepness * sinf), d.x * (steepness * cosf), -d.x * d.y * (steepness * sinf));

    binormal +=
        float3(-d.x * d.y * (steepness * sinf), d.y * (steepness * cosf), -d.y * d.y * (steepness * sinf));

    return float3(d.x * (a * cosf), a * sinf, d.y * (a * cosf));
}

// 頂点の変形と座標変換

PS_INPUT VS(VS_INPUT input)
{
    const float minimumWavelength = 1e-4f;
    const float gridWaveFadeStart = 0.20f;
    const float gridWaveFadeEnd = 0.45f;
    const float distanceFadeStartWavelengths = 16.0f;
    const float distanceFadeEndWavelengths = 64.0f;
    PS_INPUT result;

    float3 wavePos = input.position + oceanOffset;

    float3 pos = wavePos;

    // 接線と従法線

    float3 tangent = float3(1.0f, 0.0f, 0.0f);
    float3 binormal = float3(0.0f, 0.0f, 1.0f);

    // 波の設定

    float2 baseWindDir = normalize(float2(1.0f, kOceanWindZ));
    float wavelength = max(oceanWaveLength, minimumWavelength);
    float steepness = max(oceanWaveSteepness, 0.0f);

    const float gridHalfIntervals = (kOceanGridResolution - 1) * 0.5f;
    // 頂点を増やさずに、257点の指数格子へ合わせる。短い波は、
    // 疎な外側のセルが変位をサンプリングしきれなくなる前に、減衰させる
    float gridBase = oceanSize * 0.5f / (exp(kOceanGridExponent) - 1.0f);
    float gridSpacing = (max(abs(input.position.x), abs(input.position.z)) + gridBase) *
        (exp(kOceanGridExponent / gridHalfIntervals) - 1.0f);

    // 複数の波を重ねる

    for (int i = 0; i < kOceanWaveCount; ++i)
    {
        float phaseOffset = (float)i * kOceanWavePhaseStride;
        float randomAngle = sin((float)i * kOceanWaveDirectionStride) * kOceanWaveDirectionSpread;

        float s_rot = sin(randomAngle);
        float c_rot = cos(randomAngle);

        float2 waveDir = mul(float2x2(c_rot, -s_rot, s_rot, c_rot), baseWindDir);

        float resolvedWave = 1.0f - smoothstep(gridWaveFadeStart, gridWaveFadeEnd, gridSpacing / wavelength);
        pos += CalculateGerstnerWave(waveDir, steepness * resolvedWave, wavelength,
            wavePos, tangent, binormal, phaseOffset);

        wavelength *= kOceanWavelengthDecay;
        steepness *= kOceanWaveSteepnessDecay;
    }

    pos.y -= oceanWaveHeightOffset;
    // 遠方の波はサブピクセルになる。疎な外側の格子でエイリアシングが出る前に、幾何的な変位を減衰させる
    float waveDetail = 1.0f - smoothstep(
        oceanWaveLength * distanceFadeStartWavelengths, oceanWaveLength * distanceFadeEndWavelengths,
        length(input.position.xz));
    pos = lerp(float3(wavePos.x, -oceanWaveHeightOffset, wavePos.z), pos, waveDetail);
    tangent = lerp(float3(1, 0, 0), tangent, waveDetail);
    binormal = lerp(float3(0, 0, 1), binormal, waveDetail);

    // 法線を計算する

    float3 normal = normalize(cross(binormal, tangent));

    // ワールド座標へ変換する

    float3 worldPos = pos;

    // 描画に必要な値を出力する

    result.position = mul(float4(worldPos, 1.0f), mvp);
    result.worldPos = worldPos;
    result.wavePos = wavePos;
    result.normal = normalize(normal);
    result.tangent = normalize(tangent);
    result.texcoord = input.texcoord;

    return result;
}
