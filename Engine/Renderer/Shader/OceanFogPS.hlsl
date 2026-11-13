#include "OceanFog.hlsli"
struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};
float4 PS(PS_INPUT input) : SV_TARGET
{
    return ApplyOceanFog(input.texcoord, max(abs(ddy(input.texcoord.y)), 1e-6f));
}
