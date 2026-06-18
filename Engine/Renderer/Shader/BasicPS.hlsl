#include "Common.hlsli"

Texture2D gDiffuse : register(t0);
SamplerState gSampler : register(s0);

float4 PS(PS_INPUT input) : SV_TARGET
{
    // テクスチャのサンプリング結果を返す
    return gDiffuse.Sample(gSampler, input.texcoord);
}
