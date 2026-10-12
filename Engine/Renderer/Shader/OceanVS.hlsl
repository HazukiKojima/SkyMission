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

static const float PI = 3.14159265359f;
static const float TWO_PI = 6.28318530718f;

// ============================================================
// Random
// ============================================================

uint HashWaveSeed(uint value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float RandomWave(uint seed)
{
    return float(HashWaveSeed(seed) & 0x00ffffffu) / 16777216.0f;
}

// ============================================================
// Gerstner Wave
// ============================================================

float3 CalculateGerstnerWave(
    float2 dir,
    float steepness,
    float wavelength,
    float amplitudeScale,
    float3 p,
    inout float3 tangent,
    inout float3 binormal,
    float phaseOffset,
    float speedScale)
{
    float k = TWO_PI / wavelength;
    float c = sqrt(9.81f / k) * speedScale;
    float2 d = normalize(dir);

    float waveSteepness = steepness * amplitudeScale;
    float a = steepness / k * amplitudeScale;

    float f = k * (dot(d, p.xz) - c * time) + phaseOffset;

    float sinf = sin(f);
    float cosf = cos(f);

    tangent += float3(
        -d.x * d.x * waveSteepness * sinf,
         d.x * waveSteepness * cosf,
        -d.x * d.y * waveSteepness * sinf
    );

    binormal += float3(
        -d.x * d.y * waveSteepness * sinf,
         d.y * waveSteepness * cosf,
        -d.y * d.y * waveSteepness * sinf
    );

    return float3(
        d.x * a * cosf,
        a * sinf,
        d.y * a * cosf
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

    float2 baseWindDir = normalize(float2(1.0f, 0.6f));

    const int NUM_WAVES = 8;

    // ========================================================
    // Gerstner Waves
    // ========================================================

    [unroll]
    for (int i = 0; i < NUM_WAVES; ++i)
    {
        uint seed = (uint) i + 1u;

        float wavelengthRandom = RandomWave(seed * 17u + 11u);
        float directionRandom = RandomWave(seed * 17u + 23u);
        float amplitudeRandom = RandomWave(seed * 17u + 37u);
        float phaseRandom = RandomWave(seed * 17u + 53u);
        float speedRandom = RandomWave(seed * 17u + 71u);

        // 長いうねりから短い波までを分散
        float wavelength = exp(
            lerp(log(120.0f), log(1800.0f), wavelengthRandom)
        );

        // 頂点変位を抑え、巨大な波を防ぐ
        float amplitudeScale = lerp(
            0.30f,
            0.75f,
            amplitudeRandom
        );

        float steepness = lerp(
            0.008f,
            0.025f,
            amplitudeRandom
        );

        // 主風向きの周囲に波を分散させる
        // 横風や逆方向の波を固定グループにしない
        float angle = (directionRandom - 0.5f) * 2.6f;

        float s = sin(angle);
        float c = cos(angle);

        float2 waveDir = mul(
            float2x2(c, -s, s, c),
            baseWindDir
        );

        float speedScale = lerp(
            0.85f,
            1.15f,
            speedRandom
        );

        float phaseOffset = phaseRandom * TWO_PI;

        // 距離による変位量の変更を行わない
        // 同じワールド座標ではカメラ位置に関係なく同じ波形になる
        pos += CalculateGerstnerWave(
            waveDir,
            steepness,
            wavelength,
            amplitudeScale,
            wavePos,
            tangent,
            binormal,
            phaseOffset,
            speedScale
        );
    }

    // ========================================================
    // Normal
    // ========================================================

    float3 normal = normalize(cross(binormal, tangent));

    // ========================================================
    // Output
    // ========================================================

    result.position = mul(float4(pos, 1.0f), mvp);
    result.worldPos = pos;
    result.wavePos = wavePos;
    result.normal = normal;
    result.tangent = normalize(tangent);
    result.texcoord = input.texcoord;

    return result;
}