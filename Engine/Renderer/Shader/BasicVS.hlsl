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
    float time : TEXCOORD1;
    float3 worldPos : TEXCOORD2;
    float3 normal : NORMAL;
};

// ゲルストナー波を1つ計算して変位と法線用ベクトルを返す関数
float3 CalculateGerstnerWave(float2 dir, float steepness, float wavelength, float3 p, inout float3 tangent, inout float3 binormal, float phaseOffset)
{
    float k = 2.0f * 3.14159f / wavelength;
    float c = sqrt(9.8f / k);
    float2 d = normalize(dir);
    
    // ★時間の進行に加えて、波ごとにランダムな初期位置（phaseOffset）を足して線感を消す
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

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT result;
    float3 pos = input.position;
    
    float3 tangent = float3(1.0f, 0.0f, 0.0f);
    float3 binormal = float3(0.0f, 0.0f, 1.0f);
    float3 p = pos;
    
    float2 baseWindDir = normalize(float2(1.0f, 0.6f));
    float wavelength = 18.0f; // 少し大きめのうねりからスタート
    float steepness = 0.22f;
    
    const int NUM_WAVES = 6;
    
    for (int i = 0; i < NUM_WAVES; ++i)
    {
        // ★規則性を壊すための魔法の数字（適当な無理数を掛けてバラバラのオフセットを作る）
        float phaseOffset = (float) i * 21.53f;
        
        // ★方向のズレも、単調な扇形ではなく不規則に散らす
        // sin(i * 大きい数字) を使うことで、疑似的にランダムな振れ幅（-0.5?0.5ラジアン程度）を作ります
        float randomAngle = sin((float) i * 7.3f) * 0.5f;
        
        float s_rot = sin(randomAngle);
        float c_rot = cos(randomAngle);
        float2 waveDir = mul(float2x2(c_rot, -s_rot, s_rot, c_rot), baseWindDir);
        
        // phaseOffsetを渡して計算
        pos += CalculateGerstnerWave(waveDir, steepness, wavelength, p, tangent, binormal, phaseOffset);
        
        // 次の波へ
        wavelength *= 0.55f;
        steepness *= 0.75f;
    }
    
    float3 normal = normalize(cross(binormal, tangent));

    result.position = mul(float4(pos, 1.0f), mvp);
    result.worldPos = pos;
    result.normal = normal;
    result.texcoord = input.texcoord;
    result.time = time;
    
    return result;
}