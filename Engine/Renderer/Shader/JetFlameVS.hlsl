cbuffer FlameBuffer : register(b0)
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
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    float pulse = 0.9f + 0.12f * sin(time * 38.0f);
    float normalizedLength = saturate(input.texcoord.y);
    float sway = sin(time * 29.0f + input.position.z * 4.0f) * 0.06f * normalizedLength;

    float3 position = input.position;
    position.x *= pulse * (1.0f - normalizedLength * 0.2f);
    position.x += sway;

    output.position = mul(float4(position, 1.0f), mvp);
    output.texcoord = input.texcoord;
    return output;
}
