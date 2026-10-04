cbuffer MatrixBuffer : register(b0)
{
    float4x4 mvp;
    float time;
    float3 padding;
    float3 cameraPos;
    float pad2;
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
    float3 worldPos : TEXCOORD1;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;

    float3 worldPos = input.position + cameraPos;

    output.position = mul(
        float4(worldPos, 1.0f),
        mvp
    );

    output.worldPos = worldPos;
    output.texcoord = input.texcoord;

    return output;
}