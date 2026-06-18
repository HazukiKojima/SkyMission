#include "Common.hlsli"

struct VS_INPUT
{
    float3 position : POSITION;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT result;
    result.position = float4(input.position, 1.0f);
    return result;
}
