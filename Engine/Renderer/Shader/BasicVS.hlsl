cbuffer MatrixBuffer : register(b0)
{
    float4x4 mvp;
    float time;
    float3 padding;
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
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT result;
    float3 pos = input.position;

    // 波のパラメータ
    float steepness = 0.5f; // 波の鋭さ
    float wavelength = 3.0f;
    float k = 2.0f * 3.14159f / wavelength;
    float speed = 2.0f;
    float phase = speed * time;

    // ガースナー波の計算
    float angle = k * (pos.x + pos.z) - phase;
    float s = sin(angle);
    float c = cos(angle);

// 複数の波を合成する
    float timeVal = time * 2.0f;
    pos.y += sin(pos.x * 1.5f + timeVal) * 0.15f; // 波1
    pos.y += sin(pos.z * 1.2f + timeVal * 1.2f) * 0.1f; // 波2（周期と速度をずらす）
    pos.y += sin((pos.x + pos.z) * 0.8f + timeVal * 0.8f) * 0.05f; // 波3（斜めの波）

    result.position = mul(float4(pos, 1.0f), mvp);
    result.texcoord = input.texcoord;
    result.time = time;
    
    return result;
}