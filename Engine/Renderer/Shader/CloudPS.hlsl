#include "OceanFog.hlsli"
#include "VolumetricClouds.hlsli"
struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};
float4 PS(PS_INPUT input) : SV_TARGET
{
    return IntegrateClouds(input.texcoord, cloudQuality.w, input.position.xy);
}
