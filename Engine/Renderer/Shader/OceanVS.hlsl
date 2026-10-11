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

// ============================================================
// Gerstner Wave
// ============================================================

float3 CalculateGerstnerWave(
float2 dir,
float steepness,
float wavelength,
float3 p,
inout float3 tangent,
inout float3 binormal,
float phaseOffset)
{
    float k = 2.0f * 3.14159265359f / wavelength;
    float c = sqrt(9.8f / k);
    float2 d = normalize(dir);

    float f = k * (dot(d, p.xz) - c * time * 0.8f) + phaseOffset;
    float a = steepness / k;

    float sinf = sin(f);
    float cosf = cos(f);

    tangent += float3(
    -d.x * d.x * (steepness * sinf),
     d.x * (steepness * cosf),
    -d.x * d.y * (steepness * sinf)
);

    binormal += float3(
    -d.x * d.y * (steepness * sinf),
     d.y * (steepness * cosf),
    -d.y * d.y * (steepness * sinf)
);

    return float3(
    d.x * (a * cosf),
    a * sinf,
    d.y * (a * cosf)
);

}

// ============================================================
// Vertex Shader
// ============================================================

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT result;

// カメラ追従メッシュのローカル座標をワールド座標に変換
    float3 wavePos = input.position + oceanOffset;
    float3 pos = wavePos;

// ========================================================
// Tangent / Binormal
// ========================================================

    float3 tangent = float3(1.0f, 0.0f, 0.0f);
    float3 binormal = float3(0.0f, 0.0f, 1.0f);

// ========================================================
// Wave Settings
// ========================================================

// 主風向を維持しながら、波ごとに方向を変える
    float2 baseWindDir = normalize(float2(1.0f, 0.6f));

    const int NUM_WAVES = 8;

// 規則的な比率ではなく、個別に波長を設定
    const float wavelengths[8] =
    {
        85.0f,
    61.0f,
    46.0f,
    37.0f,
    31.0f,
    27.0f,
    23.0f,
    19.0f
    };

// 大きなうねりを抑え、細かい波ほど振幅を小さくする
    const float steepnesses[8] =
    {
        0.045f,
    0.035f,
    0.027f,
    0.020f,
    0.015f,
    0.011f,
    0.008f,
    0.006f
    };

// 主風向からの角度
    const float directionAngles[8] =
    {
        -0.55f,
     0.28f,
    -0.16f,
     0.68f,
    -0.82f,
     0.43f,
     0.91f,
    -0.35f
    };

// 波ごとに異なる位相
    const float phaseOffsets[8] =
    {
        0.7f,
    3.1f,
    5.4f,
    1.8f,
    4.6f,
    2.3f,
    5.9f,
    0.2f
    };

// ========================================================
// Gerstner Waves
// ========================================================

    for (int i = 0; i < NUM_WAVES; ++i)
    {
        float angle = directionAngles[i];

        float s_rot = sin(angle);
        float c_rot = cos(angle);

        float2 waveDir = mul(
        float2x2(
            c_rot, -s_rot,
            s_rot, c_rot
        ),
        baseWindDir
    );

        pos += CalculateGerstnerWave(
        waveDir,
        steepnesses[i],
        wavelengths[i],
        wavePos,
        tangent,
        binormal,
        phaseOffsets[i]
    );
    }

// ========================================================
// Normal
// ========================================================

    float3 normal = normalize(cross(binormal, tangent));

// ========================================================
// World Position
// ========================================================

    float3 worldPos = pos;

// ========================================================
// Output
// ========================================================

    result.position = mul(float4(worldPos, 1.0f), mvp);
    result.worldPos = worldPos;
    result.wavePos = wavePos;
    result.normal = normalize(normal);
    result.tangent = normalize(tangent);
    result.texcoord = input.texcoord;

    return result;
}
