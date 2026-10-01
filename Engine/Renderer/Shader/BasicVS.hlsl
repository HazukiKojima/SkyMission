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

<<<<<<< Updated upstream
// ?Q???X?g?i?[?g??1??v?Z????��??@???p?x?N?g?????????
=======
// ============================================================
// Gerstner Wave
// ============================================================

>>>>>>> Stashed changes
float3 CalculateGerstnerWave(float2 dir, float steepness, float wavelength, float3 p, inout float3 tangent, inout float3 binormal, float phaseOffset)
{
    float k = 2.0f * 3.14159265359f / wavelength;
    float c = sqrt(9.8f / k);
    float2 d = normalize(dir);
<<<<<<< Updated upstream
    
    // ???????i?s???????A?g?????????_?????????u?iphaseOffset?j??????????????
=======

>>>>>>> Stashed changes
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
<<<<<<< Updated upstream
    float3 pos = input.position;
    
    float3 tangent = float3(1.0f, 0.0f, 0.0f);
    float3 binormal = float3(0.0f, 0.0f, 1.0f);
    float3 p = pos;
    
=======

    // --------------------------------------------------------
    // 波計算用座標
    // --------------------------------------------------------

    float3 wavePos = input.position;

    // --------------------------------------------------------
    // 海面の初期位置
    // --------------------------------------------------------

    float3 pos = wavePos;

    // --------------------------------------------------------
    // Tangent / Binormal
    // --------------------------------------------------------

    float3 tangent = float3(1.0f, 0.0f, 0.0f);
    float3 binormal = float3(0.0f, 0.0f, 1.0f);

    // --------------------------------------------------------
    // Wave Settings
    // --------------------------------------------------------

>>>>>>> Stashed changes
    float2 baseWindDir = normalize(float2(1.0f, 0.6f));
    float wavelength = 30.0f;
    float steepness = 0.055f;

    const int NUM_WAVES = 6;

    // --------------------------------------------------------
    // Gerstner Waves
    // --------------------------------------------------------

    for (int i = 0; i < NUM_WAVES; ++i)
    {
<<<<<<< Updated upstream
        // ???K??????????????@??????i?K???????????|????o???o????I?t?Z?b?g????j
        float phaseOffset = (float) i * 21.53f;
        
        // ????????Y????A?P?????`??????s?K????U??
        // sin(i * ????????) ??g???????A?^???I??????_????U???i-0.5?0.5???W?A?????x?j??????
=======
        float phaseOffset = (float) i * 21.53f;
>>>>>>> Stashed changes
        float randomAngle = sin((float) i * 7.3f) * 1.1f;

        float s_rot = sin(randomAngle);
        float c_rot = cos(randomAngle);
<<<<<<< Updated upstream
        float2 waveDir = mul(float2x2(c_rot, -s_rot, s_rot, c_rot), baseWindDir);
        
        // phaseOffset??n????v?Z
        pos += CalculateGerstnerWave(waveDir, steepness, wavelength, p, tangent, binormal, phaseOffset);
        
        // ????g??
=======

        float2 waveDir = mul(
            float2x2(
                c_rot, -s_rot,
                s_rot, c_rot
            ),
            baseWindDir
        );

        pos += CalculateGerstnerWave(
            waveDir,
            steepness,
            wavelength,
            wavePos,
            tangent,
            binormal,
            phaseOffset
        );

>>>>>>> Stashed changes
        wavelength *= 0.82f;
        steepness *= 0.72f;
    }

    // --------------------------------------------------------
    // Normal
    // --------------------------------------------------------

    float3 normal = normalize(cross(binormal, tangent));

<<<<<<< Updated upstream
    result.position = mul(float4(pos, 1.0f), mvp);
    result.worldPos = pos;
    result.normal = normal;
=======
    // --------------------------------------------------------
    // World Position
    // --------------------------------------------------------
    //
    // X方向にはcameraPosを加えない。
    //
    // cameraPos.xを加えると、
    // A/Dによるカメラ移動と海面移動が相殺される。
    //

    float3 worldPos = pos;

    // --------------------------------------------------------
    // Output
    // --------------------------------------------------------

    result.position = mul(float4(worldPos, 1.0f), mvp);
    result.worldPos = worldPos;
    result.wavePos = wavePos;
    result.normal = normalize(normal);
>>>>>>> Stashed changes
    result.tangent = normalize(tangent);
    result.texcoord = input.texcoord;

    return result;
}