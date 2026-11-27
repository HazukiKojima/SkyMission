#include "OceanFog.hlsli"

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float4 PS(PS_INPUT input) : SV_TARGET
{
    const float minimumPixelUvHeight = 1e-6f;
    return ApplyOceanFog(input.texcoord, max(abs(ddy(input.texcoord.y)), minimumPixelUvHeight));
}
