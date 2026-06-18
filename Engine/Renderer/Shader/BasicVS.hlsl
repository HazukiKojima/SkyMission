#include "Common.hlsli"

struct VS_INPUT
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0; // 頂点にテクスチャ座標を追加
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT result;
    result.position = float4(input.position, 1.0f);
    result.texcoord = input.texcoord;
    return result;
}
