#include "../Environment/OceanWaveConstants.h"

static const float kBasicWaveTimeScale = 0.8f;
static const float kBasicWaveDirectionSpread = 1.1f;
static const float kBasicWaveLength = 30.0f;
static const float kBasicWaveSteepness = 0.055f;

cbuffer MatrixBuffer : register(b0)
{
    float4x4 mvp;

    float time;
    float3 padding;

    float3 cameraPos;
    float pad2;
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

    float f = k * (dot(d, p.xz) - c * time * kBasicWaveTimeScale) + phaseOffset;
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

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT result;

    float3 wavePos = input.position;

    float3 pos = wavePos;

    float3 tangent = float3(1.0f, 0.0f, 0.0f);
    float3 binormal = float3(0.0f, 0.0f, 1.0f);

    float2 baseWindDir = normalize(float2(1.0f, kOceanWindZ));
    float wavelength = kBasicWaveLength;
    float steepness = kBasicWaveSteepness;



    // 向きと波長の異なる6成分の波を重ねる
    for (int i = 0; i < kOceanWaveCount; ++i)
    {
        float phaseOffset = (float)i * kOceanWavePhaseStride;
        float randomAngle = sin((float)i * kOceanWaveDirectionStride) * kBasicWaveDirectionSpread;

        float s_rot = sin(randomAngle);
        float c_rot = cos(randomAngle);

        float2 waveDir = mul(float2x2(c_rot, -s_rot, s_rot, c_rot), baseWindDir);

        pos += CalculateGerstnerWave(waveDir, steepness, wavelength, wavePos, tangent, binormal, phaseOffset);

        wavelength *= kOceanWavelengthDecay;
        steepness *= kOceanWaveSteepnessDecay;
    }

    // 重ねた波の接線から法線を求めてピクセルシェーダーへ渡す
    float3 normal = normalize(cross(binormal, tangent));

    float3 worldPos = pos;

    result.position = mul(float4(worldPos, 1.0f), mvp);
    result.worldPos = worldPos;
    result.wavePos = wavePos;
    result.normal = normalize(normal);
    result.tangent = normalize(tangent);
    result.texcoord = input.texcoord;

    return result;
}