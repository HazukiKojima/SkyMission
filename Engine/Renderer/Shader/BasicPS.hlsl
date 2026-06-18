#include "Common.hlsli"

float4 PS(PS_INPUT input) : SV_TARGET
{
    return float4(1.0f, 0.8f, 0.2f, 1.0f); // 黄色
}