#include "Common.hlsli"

// 定数バッファ: b0 に MVP 行列を配置
cbuffer MatrixBuffer : register(b0)
{
    float4x4 mvp;
}

struct VS_INPUT
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT result;
    // 頂点をワールド→ビュー→プロジェクションで変換
    result.position = mul(float4(input.position, 1.0f), mvp);
    result.texcoord = input.texcoord;
    return result;
}
