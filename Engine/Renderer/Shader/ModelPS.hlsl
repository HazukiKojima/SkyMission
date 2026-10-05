Texture2D gDiffuse : register(t0);
SamplerState gSampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float4 PS(PS_INPUT input) : SV_TARGET
{
    return gDiffuse.Sample(gSampler, input.texcoord);
}
